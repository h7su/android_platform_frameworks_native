/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#[derive(Debug)]
pub struct TipcError;

impl TipcError {
    pub fn from_uapi(x: i32) -> Self {
        TipcError
    }
}

pub type Result<T> = core::result::Result<T, TipcError>;

pub enum ConnectResult<T> {
    Accept(T),
}

pub struct Handle;

impl Handle {
    pub fn connect(_port: &core::ffi::CStr) -> Result<Self> {
        Err(TipcError)
    }

    pub fn as_raw_fd(&self) -> i32 {
        -1
    }
}

pub enum MessageResult {
    MaintainConnection,
}

pub struct PortCfg;

pub trait UnbufferedService {
    type Connection;

    fn on_connect(
        &self,
        _port: &PortCfg,
        handle: &Handle,
        peer: &Uuid,
    ) -> Result<ConnectResult<Self::Connection>>;

    fn on_message(
        &self,
        conn: &Self::Connection,
        _handle: &Handle,
        buffer: &mut [u8],
    ) -> Result<MessageResult>;

    fn on_disconnect(&self, conn: &Self::Connection);
}

pub struct Uuid;

impl Uuid {
    pub fn as_ptr(&self) -> *const u8 {
        core::ptr::null()
    }
}
