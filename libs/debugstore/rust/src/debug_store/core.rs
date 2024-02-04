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
use super::debug_store_event::DebugStoreEvent;
use super::debug_store_storage::DebugStoreStorage;
use super::event_type::EventType;
use once_cell::sync::Lazy;
use std::fmt;
use std::sync::{
    atomic::{AtomicU64, Ordering},
    Arc,
};
use std::time::Instant;
use std::time::SystemTime;

//  Lazily initialized static instance of DebugStore.
static INSTANCE: Lazy<Arc<DebugStore>> = Lazy::new(|| {
    Arc::new(DebugStore::new(DebugStore::DEFAULT_EVENT_LIMIT, DebugStore::MAX_DELAY_MS))
});

/// The `DebugStore` struct is responsible for managing debug events and data.
pub struct DebugStore {
    /// Atomic counter for generating unique event IDs.
    id_generator: AtomicU64,
    /// Atomic counter for tracking the total number of entries over the lifetime of the event store.
    total_entries: AtomicU64,
    /// Non-blocking storage for debug events.
    event_store: DebugStoreStorage<DebugStoreEvent>,
}

impl DebugStore {
    const DEFAULT_EVENT_LIMIT: usize = 16;
    const NON_CLOSABLE_ID: u64 = 0;
    const MAX_DELAY_MS: u64 = 20;
    const ENCODE_VERSION: u32 = 1;

    /// Creates a new instance of `DebugStore` with specified event limit and maximum delay.
    fn new(event_limit: usize, max_delay_ms: u64) -> Self {
        Self {
            id_generator: AtomicU64::new(1),
            total_entries: AtomicU64::new(0),
            event_store: DebugStoreStorage::new(event_limit, max_delay_ms),
        }
    }

    /// Returns a shared instance of `DebugStore`.
    ///
    /// This method provides a singleton pattern access to `DebugStore`.
    pub fn get_instance() -> Arc<Self> {
        INSTANCE.clone()
    }

    /// Begins a new debug event with the given name and data.
    ///
    /// This method logs the start of a debug event, assigning it a unique ID and marking its start time.
    /// - `name`: The name of the debug event.
    /// - `data`: Associated data as key-value pairs.
    /// - Returns: A unique ID for the debug event.
    pub fn begin(&self, name: String, data: Vec<(String, String)>) -> u64 {
        let id = self.generate_id();
        if self
            .event_store
            .insert(DebugStoreEvent::new(
                id,
                Some(name),
                Instant::now(),
                EventType::DurationStart,
                data,
            ))
            .is_ok()
        {
            self.total_entries.fetch_add(1, Ordering::Relaxed);
            id
        } else {
            Self::NON_CLOSABLE_ID
        }
    }

    /// Records a debug event without a specific duration, with the given name and data.
    ///
    /// This method logs an instantaneous debug event, useful for events that don't have a duration but are significant.
    /// - `name`: The name of the debug event.
    /// - `data`: Associated data as key-value pairs.
    pub fn record(&self, name: String, data: Vec<(String, String)>) {
        let _ = self.event_store.insert(DebugStoreEvent::new(
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
            let _ = self.event_store.insert(DebugStoreEvent::new(
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
        let total_entries = self.total_entries.load(Ordering::Relaxed);
        let lock_misses = self.event_store.get_lock_misses();
        let uptime_now = SystemTime::now()
            .duration_since(SystemTime::UNIX_EPOCH)
            .unwrap_or_default()
            .as_millis();
        let length = match self.event_store.len() {
            Ok(length) => length as isize,
            Err(_) => -1,
        };

        write!(
            f,
            "{},{},{},{},{}::",
            Self::ENCODE_VERSION,
            total_entries,
            length,
            lock_misses,
            uptime_now
        )?;

        if let Some(events_string) = self.event_store.fold(String::new(), |mut acc, event| {
            if !acc.is_empty() {
                acc.push_str("||");
            }
            acc.push_str(&event.to_string());
            acc
        }) {
            write!(f, "{}", events_string)
        } else {
            write!(f, "-")
        }
    }
}
