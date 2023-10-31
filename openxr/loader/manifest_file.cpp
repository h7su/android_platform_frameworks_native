// Copyright (c) 2017-2023, The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Initial Authors: Mark Young <marky@lunarg.com>, Dave Houlton <daveh@lunarg.com>
//

#include "manifest_file.hpp"

#ifdef OPENXR_HAVE_COMMON_CONFIG
#include "common_config.h"
#endif  // OPENXR_HAVE_COMMON_CONFIG

#include "filesystem_utils.hpp"
#include "loader_logger.hpp"
#include "unique_asset.h"

#include <json/json.h>
#include <openxr/openxr.h>

#include <sys/stat.h>

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// OpenXR paths and registry key locations
#define OPENXR_RELATIVE_PATH "openxr/"
#define OPENXR_IMPLICIT_API_LAYER_RELATIVE_PATH "/api_layers/implicit.d"
#define OPENXR_EXPLICIT_API_LAYER_RELATIVE_PATH "/api_layers/explicit.d"

#define PATH_SEPARATOR ':'
#define DIRECTORY_SYMBOL '/'

#if defined(__x86_64__) && defined(__ILP32__)
#define XR_ARCH_ABI "x32"
#elif defined(_M_X64) || defined(__x86_64__)
#define XR_ARCH_ABI "x86_64"
#elif (defined(__aarch64__) && defined(__LP64__)) || defined(_M_ARM64)
#define XR_ARCH_ABI "aarch64"
#elif (defined(__ARM_ARCH) && __ARM_ARCH >= 7 && (defined(__ARM_PCS_VFP) || defined(__ANDROID__))) || defined(_M_ARM)
#define XR_ARCH_ABI "armv7a-vfp"
#endif

#include <android/asset_manager.h>

#ifdef XRLOADER_DISABLE_EXCEPTION_HANDLING
#if JSON_USE_EXCEPTIONS
#error \
    "Loader is configured to not catch exceptions, but jsoncpp was built with exception-throwing enabled, which could violate the C ABI. One of those two things needs to change."
#endif  // JSON_USE_EXCEPTIONS
#endif  // !XRLOADER_DISABLE_EXCEPTION_HANDLING

#include "runtime_interface.hpp"

// Utility functions for finding files in the appropriate paths

static inline bool StringEndsWith(const std::string &value, const std::string &ending) {
    if (ending.size() > value.size()) {
        return false;
    }
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

// If the file found is a manifest file name, add it to the out_files manifest list.
static void AddIfJson(const std::string &full_file, std::vector<std::string> &manifest_files) {
    if (full_file.empty() || !StringEndsWith(full_file, ".json")) {
        return;
    }
    manifest_files.push_back(full_file);
}

// Check the current path for any manifest files.  If the provided search_path is a directory, look for
// all included JSON files in that directory.  Otherwise, just check the provided search_path which should
// be a single filename.
static void CheckAllFilesInThePath(const std::string &search_path, bool is_directory_list,
                                   std::vector<std::string> &manifest_files) {
    if (FileSysUtilsPathExists(search_path)) {
        std::string absolute_path;
        if (!is_directory_list) {
            // If the file exists, try to add it
            if (FileSysUtilsIsRegularFile(search_path)) {
                FileSysUtilsGetAbsolutePath(search_path, absolute_path);
                AddIfJson(absolute_path, manifest_files);
            }
        } else {
            std::vector<std::string> files;
            if (FileSysUtilsFindFilesInPath(search_path, files)) {
                for (std::string &cur_file : files) {
                    std::string relative_path;
                    FileSysUtilsCombinePaths(search_path, cur_file, relative_path);
                    if (!FileSysUtilsGetAbsolutePath(relative_path, absolute_path)) {
                        continue;
                    }
                    AddIfJson(absolute_path, manifest_files);
                }
            }
        }
    }
}

// Add all manifest files in the provided paths to the manifest_files list.  If search_path
// is made up of directory listings (versus direct manifest file names) search each path for
// any manifest files.
static void AddFilesInPath(const std::string &search_path, bool is_directory_list, std::vector<std::string> &manifest_files) {
    std::size_t last_found = 0;
    std::size_t found = search_path.find_first_of(PATH_SEPARATOR);
    std::string cur_search;

    // Handle any path listings in the string (separated by the appropriate path separator)
    while (found != std::string::npos) {
        // substr takes a start index and length.
        std::size_t length = found - last_found;
        cur_search = search_path.substr(last_found, length);

        CheckAllFilesInThePath(cur_search, is_directory_list, manifest_files);

        // This works around issue if multiple path separator follow each other directly.
        last_found = found;
        while (found == last_found) {
            last_found = found + 1;
            found = search_path.find_first_of(PATH_SEPARATOR, last_found);
        }
    }

    // If there's something remaining in the string, copy it over
    if (last_found < search_path.size()) {
        cur_search = search_path.substr(last_found);
        CheckAllFilesInThePath(cur_search, is_directory_list, manifest_files);
    }
}

// Copy all paths listed in the cur_path string into output_path and append the appropriate relative_path onto the end of each.
static void CopyIncludedPaths(bool is_directory_list, const std::string &cur_path, const std::string &relative_path,
                              std::string &output_path) {
    if (!cur_path.empty()) {
        std::size_t last_found = 0;
        std::size_t found = cur_path.find_first_of(PATH_SEPARATOR);

        // Handle any path listings in the string (separated by the appropriate path separator)
        while (found != std::string::npos) {
            std::size_t length = found - last_found;
            output_path += cur_path.substr(last_found, length);
            if (is_directory_list && (cur_path[found - 1] != '\\' && cur_path[found - 1] != '/')) {
                output_path += DIRECTORY_SYMBOL;
            }
            output_path += relative_path;
            output_path += PATH_SEPARATOR;

            last_found = found;
            found = cur_path.find_first_of(PATH_SEPARATOR, found + 1);
        }

        // If there's something remaining in the string, copy it over
        size_t last_char = cur_path.size() - 1;
        if (last_found != last_char) {
            output_path += cur_path.substr(last_found);
            if (is_directory_list && (cur_path[last_char] != '\\' && cur_path[last_char] != '/')) {
                output_path += DIRECTORY_SYMBOL;
            }
            output_path += relative_path;
            output_path += PATH_SEPARATOR;
        }
    }
}

// Look for data files in the provided paths
static void ReadDataFilesInSearchPaths(const std::string &relative_path,
                                       std::vector<std::string> &manifest_files) {
    std::string search_path;

    // Search order, highest priority first
    CopyIncludedPaths(true, "/odm/etc", relative_path, search_path);
    CopyIncludedPaths(true, "/vendor/etc", relative_path, search_path);
    CopyIncludedPaths(true, "/product/etc", relative_path, search_path);
    CopyIncludedPaths(true, "/system/etc", relative_path, search_path);

    // Now, parse the paths and add any manifest files found in them.
    AddFilesInPath(search_path, true, manifest_files);
}

static bool ImplTryRuntimeFilename(const char* rt_dir_prefix, uint16_t major_version, std::string& file_name) {
    auto decorated_path = rt_dir_prefix + std::to_string(major_version) + "/active_runtime." XR_ARCH_ABI ".json";
    auto undecorated_path = rt_dir_prefix + std::to_string(major_version) + "/active_runtime.json";

    struct stat buf {};
    if (0 == stat(decorated_path.c_str(), &buf)) {
        file_name = decorated_path;
        return true;
    }
    if (0 == stat(undecorated_path.c_str(), &buf)) {
        file_name = undecorated_path;
        return true;
    }
    return false;
}

// Intended to be only used as a fallback on Android, with a more open, "native" technique used in most cases
static bool PlatformGetGlobalRuntimeFileName(uint16_t major_version, std::string& file_name) {
    // Prefix for the runtime JSON file name, highest priority first
    static const char* rt_dir_prefixes[] = {"/odm", "/vendor", "/product", "/system"};

    static const std::string subdir = "/etc/openxr/";
    for (const auto prefix : rt_dir_prefixes) {
        const std::string rt_dir_prefix = prefix + subdir;
        if (ImplTryRuntimeFilename(rt_dir_prefix.c_str(), major_version, file_name)) {
            return true;
        }
    }

    return false;
}

ManifestFile::ManifestFile(ManifestFileType type, const std::string &filename, const std::string &library_path)
    : _filename(filename), _type(type), _library_path(library_path) {}

bool ManifestFile::IsValidJson(const Json::Value &root_node, JsonVersion &version) {
    if (root_node["file_format_version"].isNull() || !root_node["file_format_version"].isString()) {
        LoaderLogger::LogErrorMessage("", "ManifestFile::IsValidJson - JSON file missing \"file_format_version\"");
        return false;
    }
    std::string file_format = root_node["file_format_version"].asString();
    const int num_fields = sscanf(file_format.c_str(), "%u.%u.%u", &version.major, &version.minor, &version.patch);

    // Only version 1.0.0 is defined currently.  Eventually we may have more version, but
    // some of the versions may only be valid for layers or runtimes specifically.
    if (num_fields != 3 || version.major != 1 || version.minor != 0 || version.patch != 0) {
        std::ostringstream error_ss;
        error_ss << "ManifestFile::IsValidJson - JSON \"file_format_version\" " << version.major << "." << version.minor << "."
                 << version.patch << " is not supported";
        LoaderLogger::LogErrorMessage("", error_ss.str());
        return false;
    }

    return true;
}

static void GetExtensionProperties(const std::vector<ExtensionListing> &extensions, std::vector<XrExtensionProperties> &props) {
    for (const auto &ext : extensions) {
        auto it =
            std::find_if(props.begin(), props.end(), [&](XrExtensionProperties &prop) { return prop.extensionName == ext.name; });
        if (it != props.end()) {
            it->extensionVersion = std::max(it->extensionVersion, ext.extension_version);
        } else {
            XrExtensionProperties prop{};
            prop.type = XR_TYPE_EXTENSION_PROPERTIES;
            strncpy(prop.extensionName, ext.name.c_str(), XR_MAX_EXTENSION_NAME_SIZE - 1);
            prop.extensionName[XR_MAX_EXTENSION_NAME_SIZE - 1] = '\0';
            prop.extensionVersion = ext.extension_version;
            props.push_back(prop);
        }
    }
}

// Return any instance extensions found in the manifest files in the proper form for
// OpenXR (XrExtensionProperties).
void ManifestFile::GetInstanceExtensionProperties(std::vector<XrExtensionProperties> &props) {
    GetExtensionProperties(_instance_extensions, props);
}

const std::string &ManifestFile::GetFunctionName(const std::string &func_name) const {
    if (!_functions_renamed.empty()) {
        auto found = _functions_renamed.find(func_name);
        if (found != _functions_renamed.end()) {
            return found->second;
        }
    }
    return func_name;
}

RuntimeManifestFile::RuntimeManifestFile(const std::string &filename, const std::string &library_path)
    : ManifestFile(MANIFEST_TYPE_RUNTIME, filename, library_path) {}

static void ParseExtension(Json::Value const &ext, std::vector<ExtensionListing> &extensions) {
    Json::Value ext_name = ext["name"];
    Json::Value ext_version = ext["extension_version"];

    // Allow "extension_version" as a String or a UInt to maintain backwards compatibility, even though it should be a String.
    // Internal Issue 1411: https://gitlab.khronos.org/openxr/openxr/-/issues/1411
    // Internal MR !1867: https://gitlab.khronos.org/openxr/openxr/-/merge_requests/1867
    if (ext_name.isString() && (ext_version.isString() || ext_version.isUInt())) {
        ExtensionListing ext_listing = {};
        ext_listing.name = ext_name.asString();
        if (ext_version.isUInt()) {
            ext_listing.extension_version = ext_version.asUInt();
        } else {
            ext_listing.extension_version = atoi(ext_version.asString().c_str());
        }
        extensions.push_back(ext_listing);
    }
}

void ManifestFile::ParseCommon(Json::Value const &root_node) {
    const Json::Value &inst_exts = root_node["instance_extensions"];
    if (!inst_exts.isNull() && inst_exts.isArray()) {
        for (const auto &ext : inst_exts) {
            ParseExtension(ext, _instance_extensions);
        }
    }
    const Json::Value &funcs_renamed = root_node["functions"];
    if (!funcs_renamed.isNull() && !funcs_renamed.empty()) {
        for (Json::ValueConstIterator func_it = funcs_renamed.begin(); func_it != funcs_renamed.end(); ++func_it) {
            if (!(*func_it).isString()) {
                LoaderLogger::LogWarningMessage(
                    "", "ManifestFile::ParseCommon " + _filename + " \"functions\" section contains non-string values.");
                continue;
            }
            std::string original_name = func_it.key().asString();
            std::string new_name = (*func_it).asString();
            _functions_renamed.emplace(original_name, new_name);
        }
    }
}

void RuntimeManifestFile::CreateIfValid(std::string const &filename,
                                        std::vector<std::unique_ptr<RuntimeManifestFile>> &manifest_files) {
    std::ifstream json_stream(filename, std::ifstream::in);

    LoaderLogger::LogInfoMessage("", "RuntimeManifestFile::CreateIfValid - attempting to load " + filename);
    std::ostringstream error_ss("RuntimeManifestFile::CreateIfValid ");
    if (!json_stream.is_open()) {
        error_ss << "failed to open " << filename << ".  Does it exist?";
        LoaderLogger::LogErrorMessage("", error_ss.str());
        return;
    }
    Json::CharReaderBuilder builder;
    std::string errors;
    Json::Value root_node = Json::nullValue;
    if (!Json::parseFromStream(builder, json_stream, &root_node, &errors) || !root_node.isObject()) {
        error_ss << "failed to parse " << filename << ".";
        if (!errors.empty()) {
            error_ss << " (Error message: " << errors << ")";
        }
        error_ss << " Is it a valid runtime manifest file?";
        LoaderLogger::LogErrorMessage("", error_ss.str());
        return;
    }

    CreateIfValid(root_node, filename, manifest_files);
}

void RuntimeManifestFile::CreateIfValid(const Json::Value &root_node, const std::string &filename,
                                        std::vector<std::unique_ptr<RuntimeManifestFile>> &manifest_files) {
    std::ostringstream error_ss("RuntimeManifestFile::CreateIfValid ");
    JsonVersion file_version = {};
    if (!ManifestFile::IsValidJson(root_node, file_version)) {
        error_ss << "isValidJson indicates " << filename << " is not a valid manifest file.";
        LoaderLogger::LogErrorMessage("", error_ss.str());
        return;
    }
    const Json::Value &runtime_root_node = root_node["runtime"];
    // The Runtime manifest file needs the "runtime" root as well as a sub-node for "library_path".  If any of those aren't there,
    // fail.
    if (runtime_root_node.isNull() || runtime_root_node["library_path"].isNull() || !runtime_root_node["library_path"].isString()) {
        error_ss << filename << " is missing required fields.  Verify all proper fields exist.";
        LoaderLogger::LogErrorMessage("", error_ss.str());
        return;
    }

    std::string lib_path = runtime_root_node["library_path"].asString();

    // If the library_path variable has no directory symbol, it's just a file name and should be accessible on the
    // global library path.
    if (lib_path.find('/') != std::string::npos) {
        // If the library_path is an absolute path, just use that if it exists
        if (FileSysUtilsIsAbsolutePath(lib_path)) {
            if (!FileSysUtilsPathExists(lib_path)) {
                error_ss << filename << " library " << lib_path << " does not appear to exist";
                LoaderLogger::LogErrorMessage("", error_ss.str());
                return;
            }
        } else {
            // Otherwise, treat the library path as a relative path based on the JSON file.
            std::string canonical_path;
            std::string combined_path;
            std::string file_parent;
            // Search relative to the real manifest file, not relative to the symlink
            if (!FileSysUtilsGetCanonicalPath(filename, canonical_path)) {
                // Give relative to the non-canonical path a chance
                canonical_path = filename;
            }
            if (!FileSysUtilsGetParentPath(canonical_path, file_parent) ||
                !FileSysUtilsCombinePaths(file_parent, lib_path, combined_path) || !FileSysUtilsPathExists(combined_path)) {
                error_ss << filename << " library " << combined_path << " does not appear to exist";
                LoaderLogger::LogErrorMessage("", error_ss.str());
                return;
            }
            lib_path = combined_path;
        }
    }

    // Add this runtime manifest file
    manifest_files.emplace_back(new RuntimeManifestFile(filename, lib_path));

    // Add any extensions to it after the fact.
    // Handle any renamed functions
    manifest_files.back()->ParseCommon(runtime_root_node);
}

// Find all manifest files in the appropriate search paths/registries for the given type.
XrResult RuntimeManifestFile::FindManifestFiles(std::vector<std::unique_ptr<RuntimeManifestFile>> &manifest_files) {
    XrResult result = XR_SUCCESS;
    std::string filename;
    if (!PlatformGetGlobalRuntimeFileName(XR_VERSION_MAJOR(XR_CURRENT_API_VERSION), filename)) {
        LoaderLogger::LogErrorMessage(
            "", "RuntimeManifestFile::FindManifestFiles - failed to determine active runtime file path for this environment");
        return XR_ERROR_RUNTIME_UNAVAILABLE;
    }
    result = XR_SUCCESS;
    LoaderLogger::LogInfoMessage("", "RuntimeManifestFile::FindManifestFiles - using global runtime file " + filename);
    RuntimeManifestFile::CreateIfValid(filename, manifest_files);

    return result;
}

ApiLayerManifestFile::ApiLayerManifestFile(ManifestFileType type, const std::string &filename, const std::string &layer_name,
                                           const std::string &description, const JsonVersion &api_version,
                                           const uint32_t &implementation_version, const std::string &library_path)
    : ManifestFile(type, filename, library_path),
      _api_version(api_version),
      _layer_name(layer_name),
      _description(description),
      _implementation_version(implementation_version) {}

void ApiLayerManifestFile::AddManifestFilesAndroid(ManifestFileType type,
                                                   std::vector<std::unique_ptr<ApiLayerManifestFile>> &manifest_files) {
    AAssetManager *assetManager = (AAssetManager *)Android_Get_Asset_Manager();
    std::vector<std::string> filenames;
    {
        std::string search_path = "";
        switch (type) {
            case MANIFEST_TYPE_IMPLICIT_API_LAYER:
                search_path = "openxr/1/api_layers/implicit.d/";
                break;
            case MANIFEST_TYPE_EXPLICIT_API_LAYER:
                search_path = "openxr/1/api_layers/explicit.d/";
                break;
            default:
                return;
        }

        UniqueAssetDir dir{AAssetManager_openDir(assetManager, search_path.c_str())};
        if (!dir) {
            return;
        }
        const std::string json = ".json";
        const char *fn = nullptr;
        while ((fn = AAssetDir_getNextFileName(dir.get())) != nullptr) {
            const std::string filename = search_path + fn;
            if (filename.size() < json.size()) {
                continue;
            }
            if (filename.compare(filename.size() - json.size(), json.size(), json) == 0) {
                filenames.push_back(filename);
            }
        }
    }
    for (const auto &filename : filenames) {
        UniqueAsset asset{AAssetManager_open(assetManager, filename.c_str(), AASSET_MODE_BUFFER)};
        if (!asset) {
            LoaderLogger::LogWarningMessage(
                "", "ApiLayerManifestFile::AddManifestFilesAndroid unable to open asset " + filename + ", skipping");

            continue;
        }
        size_t length = AAsset_getLength(asset.get());
        const char *buf = reinterpret_cast<const char *>(AAsset_getBuffer(asset.get()));
        if (!buf) {
            LoaderLogger::LogWarningMessage(
                "", "ApiLayerManifestFile::AddManifestFilesAndroid unable to access asset" + filename + ", skipping");

            continue;
        }
        std::istringstream json_stream(std::string{buf, length});

        CreateIfValid(ManifestFileType::MANIFEST_TYPE_EXPLICIT_API_LAYER, filename, json_stream,
                      &ApiLayerManifestFile::LocateLibraryInAssets, manifest_files);
    }
}

void ApiLayerManifestFile::CreateIfValid(ManifestFileType type, const std::string &filename, std::istream &json_stream,
                                         LibraryLocator locate_library,
                                         std::vector<std::unique_ptr<ApiLayerManifestFile>> &manifest_files) {
    std::ostringstream error_ss("ApiLayerManifestFile::CreateIfValid ");
    Json::CharReaderBuilder builder;
    std::string errors;
    Json::Value root_node = Json::nullValue;
    if (!Json::parseFromStream(builder, json_stream, &root_node, &errors) || !root_node.isObject()) {
        error_ss << "failed to parse " << filename << ".";
        if (!errors.empty()) {
            error_ss << " (Error message: " << errors << ")";
        }
        error_ss << " Is it a valid layer manifest file?";
        LoaderLogger::LogErrorMessage("", error_ss.str());
        return;
    }
    JsonVersion file_version = {};
    if (!ManifestFile::IsValidJson(root_node, file_version)) {
        error_ss << "isValidJson indicates " << filename << " is not a valid manifest file.";
        LoaderLogger::LogErrorMessage("", error_ss.str());
        return;
    }

    Json::Value layer_root_node = root_node["api_layer"];

    // The API Layer manifest file needs the "api_layer" root as well as other sub-nodes.
    // If any of those aren't there, fail.
    if (layer_root_node.isNull() || layer_root_node["name"].isNull() || !layer_root_node["name"].isString() ||
        layer_root_node["api_version"].isNull() || !layer_root_node["api_version"].isString() ||
        layer_root_node["library_path"].isNull() || !layer_root_node["library_path"].isString() ||
        layer_root_node["implementation_version"].isNull() || !layer_root_node["implementation_version"].isString()) {
        error_ss << filename << " is missing required fields.  Verify all proper fields exist.";
        LoaderLogger::LogErrorMessage("", error_ss.str());
        return;
    }
    if (MANIFEST_TYPE_IMPLICIT_API_LAYER == type) {
        bool enabled = true;
        // Implicit layers require the disable environment variable.
        if (layer_root_node["disable_environment"].isNull() || !layer_root_node["disable_environment"].isString()) {
            error_ss << "Implicit layer " << filename << " is missing \"disable_environment\"";
            LoaderLogger::LogErrorMessage("", error_ss.str());
            return;
        }
        // Check if there's an enable environment variable provided
        if (!layer_root_node["enable_environment"].isNull() && layer_root_node["enable_environment"].isString()) {
            std::string env_var = layer_root_node["enable_environment"].asString();
            // If it's not set in the environment, disable the layer
            enabled = false;
        }
        // Check for the disable environment variable, which must be provided in the JSON
        std::string env_var = layer_root_node["disable_environment"].asString();
        // If the env var is set, disable the layer. Disable env var overrides enable above

        // Not enabled, so pretend like it isn't even there.
        if (!enabled) {
            error_ss << "Implicit layer " << filename << " is disabled";
            LoaderLogger::LogInfoMessage("", error_ss.str());
            return;
        }
    }
    std::string layer_name = layer_root_node["name"].asString();
    std::string api_version_string = layer_root_node["api_version"].asString();
    JsonVersion api_version = {};
    const int num_fields = sscanf(api_version_string.c_str(), "%u.%u", &api_version.major, &api_version.minor);
    api_version.patch = 0;

    if ((num_fields != 2) || (api_version.major == 0 && api_version.minor == 0) ||
        api_version.major > XR_VERSION_MAJOR(XR_CURRENT_API_VERSION)) {
        error_ss << "layer " << filename << " has invalid API Version.  Skipping layer.";
        LoaderLogger::LogWarningMessage("", error_ss.str());
        return;
    }

    uint32_t implementation_version = atoi(layer_root_node["implementation_version"].asString().c_str());
    std::string library_path = layer_root_node["library_path"].asString();

    // If the library_path variable has no directory symbol, it's just a file name and should be accessible on the
    // global library path.
    if (library_path.find('\\') != std::string::npos || library_path.find('/') != std::string::npos) {
        // If the library_path is an absolute path, just use that if it exists
        if (FileSysUtilsIsAbsolutePath(library_path)) {
            if (!FileSysUtilsPathExists(library_path)) {
                error_ss << filename << " library " << library_path << " does not appear to exist";
                LoaderLogger::LogErrorMessage("", error_ss.str());
                return;
            }
        } else {
            // Otherwise, treat the library path as a relative path based on the JSON file.
            std::string combined_path;
            if (!locate_library(filename, library_path, combined_path)) {
                error_ss << filename << " library " << combined_path << " does not appear to exist";
                LoaderLogger::LogErrorMessage("", error_ss.str());
                return;
            }
            library_path = combined_path;
        }
    }

    std::string description;
    if (!layer_root_node["description"].isNull() && layer_root_node["description"].isString()) {
        description = layer_root_node["description"].asString();
    }

    // Add this layer manifest file
    manifest_files.emplace_back(
        new ApiLayerManifestFile(type, filename, layer_name, description, api_version, implementation_version, library_path));

    // Add any extensions to it after the fact.
    manifest_files.back()->ParseCommon(layer_root_node);
}

void ApiLayerManifestFile::CreateIfValid(ManifestFileType type, const std::string &filename,
                                         std::vector<std::unique_ptr<ApiLayerManifestFile>> &manifest_files) {
    std::ifstream json_stream(filename, std::ifstream::in);
    if (!json_stream.is_open()) {
        std::ostringstream error_ss("ApiLayerManifestFile::CreateIfValid ");
        error_ss << "failed to open " << filename << ".  Does it exist?";
        LoaderLogger::LogErrorMessage("", error_ss.str());
        return;
    }
    CreateIfValid(type, filename, json_stream, &ApiLayerManifestFile::LocateLibraryRelativeToJson, manifest_files);
}

bool ApiLayerManifestFile::LocateLibraryRelativeToJson(
    const std::string &json_filename, const std::string &library_path,
    std::string &out_combined_path) {  // Otherwise, treat the library path as a relative path based on the JSON file.
    std::string combined_path;
    std::string file_parent;
    if (!FileSysUtilsGetParentPath(json_filename, file_parent) ||
        !FileSysUtilsCombinePaths(file_parent, library_path, combined_path) || !FileSysUtilsPathExists(combined_path)) {
        out_combined_path = combined_path;
        return false;
    }
    out_combined_path = combined_path;
    return true;
}

bool ApiLayerManifestFile::LocateLibraryInAssets(const std::string & /* json_filename */, const std::string &library_path,
                                                 std::string &out_combined_path) {
    std::string combined_path;
    std::string file_parent = GetAndroidNativeLibraryDir();
    if (!FileSysUtilsCombinePaths(file_parent, library_path, combined_path) || !FileSysUtilsPathExists(combined_path)) {
        out_combined_path = combined_path;
        return false;
    }
    out_combined_path = combined_path;
    return true;
}

void ApiLayerManifestFile::PopulateApiLayerProperties(XrApiLayerProperties &props) const {
    props.layerVersion = _implementation_version;
    props.specVersion = XR_MAKE_VERSION(_api_version.major, _api_version.minor, _api_version.patch);
    strncpy(props.layerName, _layer_name.c_str(), XR_MAX_API_LAYER_NAME_SIZE - 1);
    if (_layer_name.size() >= XR_MAX_API_LAYER_NAME_SIZE - 1) {
        props.layerName[XR_MAX_API_LAYER_NAME_SIZE - 1] = '\0';
    }
    strncpy(props.description, _description.c_str(), XR_MAX_API_LAYER_DESCRIPTION_SIZE - 1);
    if (_description.size() >= XR_MAX_API_LAYER_DESCRIPTION_SIZE - 1) {
        props.description[XR_MAX_API_LAYER_DESCRIPTION_SIZE - 1] = '\0';
    }
}

// Find all layer manifest files in the appropriate search paths/registries for the given type.
XrResult ApiLayerManifestFile::FindManifestFiles(ManifestFileType type,
                                                 std::vector<std::unique_ptr<ApiLayerManifestFile>> &manifest_files) {
    std::string relative_path;
    std::string registry_location;

    // Add the appropriate top-level folders for the relative path.  These should be
    // the string "openxr/" followed by the API major version as a string.
    relative_path = OPENXR_RELATIVE_PATH;
    relative_path += std::to_string(XR_VERSION_MAJOR(XR_CURRENT_API_VERSION));

    switch (type) {
        case MANIFEST_TYPE_IMPLICIT_API_LAYER:
            relative_path += OPENXR_IMPLICIT_API_LAYER_RELATIVE_PATH;
            break;
        case MANIFEST_TYPE_EXPLICIT_API_LAYER:
            relative_path += OPENXR_EXPLICIT_API_LAYER_RELATIVE_PATH;
            break;
        default:
            LoaderLogger::LogErrorMessage("", "ApiLayerManifestFile::FindManifestFiles - unknown manifest file requested");
            return XR_ERROR_FILE_ACCESS_ERROR;
    }

    std::vector<std::string> filenames;
    ReadDataFilesInSearchPaths(relative_path, filenames);

    for (std::string &cur_file : filenames) {
        ApiLayerManifestFile::CreateIfValid(type, cur_file, manifest_files);
    }

    ApiLayerManifestFile::AddManifestFilesAndroid(type, manifest_files);

    return XR_SUCCESS;
}
