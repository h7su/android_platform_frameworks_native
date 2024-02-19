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
use super::event::Event;
use super::event_type::EventType;
use super::storage::Storage;
use once_cell::sync::Lazy;
use std::fmt;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::Instant;
use std::time::SystemTime;

//  Lazily initialized static instance of DebugStore.
static INSTANCE: Lazy<DebugStore> = Lazy::new(DebugStore::new);

/// The `DebugStore` struct is responsible for managing debug events and data.
pub struct DebugStore {
    /// Atomic counter for generating unique event IDs.
    id_generator: AtomicU64,
    /// Non-blocking storage for debug events.
    event_store: Storage<Event, { DebugStore::DEFAULT_EVENT_LIMIT }>,
}

impl DebugStore {
    /// The default limit for the number of events that can be stored.
    ///
    /// This limit is used to initialize the storage for debug events.
    const DEFAULT_EVENT_LIMIT: usize = 16;
    /// A designated identifier used for events that cannot be closed.
    ///
    /// This ID is used for point/instantaneous events, or events do not have
    ///  a distinct end.
    const NON_CLOSABLE_ID: u64 = 0;
    /// The version number for the encoding of debug store data.
    ///
    /// This constant is used as a part of the debug store's data format,
    /// allowing for version tracking and compatibility checks.
    const ENCODE_VERSION: u32 = 1;

    /// Creates a new instance of `DebugStore` with specified event limit and maximum delay.
    fn new() -> Self {
        Self { id_generator: AtomicU64::new(1), event_store: Storage::new() }
    }

    /// Returns a shared instance of `DebugStore`.
    ///
    /// This method provides a singleton pattern access to `DebugStore`.
    pub fn get_instance() -> &'static DebugStore {
        &INSTANCE
    }

    /// Begins a new debug event with the given name and data.
    ///
    /// This method logs the start of a debug event, assigning it a unique ID and marking its start time.
    /// - `name`: The name of the debug event.
    /// - `data`: Associated data as key-value pairs.
    /// - Returns: A unique ID for the debug event.
    pub fn begin(&self, name: String, data: Vec<(String, String)>) -> u64 {
        let id = self.generate_id();
        self.event_store.insert(Event::new(
            id,
            Some(name),
            Instant::now(),
            EventType::DurationStart,
            data,
        ));
        id
    }

    /// Records a debug event without a specific duration, with the given name and data.
    ///
    /// This method logs an instantaneous debug event, useful for events that don't have a duration but are significant.
    /// - `name`: The name of the debug event.
    /// - `data`: Associated data as key-value pairs.
    pub fn record(&self, name: String, data: Vec<(String, String)>) {
        self.event_store.insert(Event::new(
            Self::NON_CLOSABLE_ID,
            Some(name),
            Instant::now(),
            EventType::Point,
            data,
        ));
    }

    /// Ends a debug event that was previously started with the given ID.
    ///
    /// This method marks the end of a debug event, completing its lifecycle.
    /// - `id`: The unique ID of the debug event to end.
    /// - `data`: Additional data to log at the end of the event.
    pub fn end(&self, id: u64, data: Vec<(String, String)>) {
        if id != Self::NON_CLOSABLE_ID {
            self.event_store.insert(Event::new(
                id,
                None,
                Instant::now(),
                EventType::DurationEnd,
                data,
            ));
        }
    }

    fn generate_id(&self) -> u64 {
        let mut id = self.id_generator.fetch_add(1, Ordering::Relaxed);
        while id == Self::NON_CLOSABLE_ID {
            id = self.id_generator.fetch_add(1, Ordering::Relaxed);
        }
        id
    }
}

impl fmt::Display for DebugStore {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let uptime_now = SystemTime::now()
            .duration_since(SystemTime::UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis();

        write!(f, "{},{},{}::", Self::ENCODE_VERSION, self.event_store.len(), uptime_now)?;

        write!(
            f,
            "{}",
            self.event_store.fold(String::new(), |mut acc, event| {
                if !acc.is_empty() {
                    acc.push_str("||");
                }
                acc.push_str(&event.to_string());
                acc
            })
        )
    }
}
