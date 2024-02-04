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

use parking_lot::Mutex;
use std::error::Error;
use std::fmt;
use std::sync::atomic::{AtomicU64, Ordering};
use std::time::Duration;

#[derive(Debug)]
pub enum DebugStoreStorageError {
    LockTimeout,
}

/// A thread-safe storage that allows non-blocking attempts to store and visit elements.
pub struct DebugStoreStorage<T> {
    /// Mutex-protected internal buffer.
    buffer: Mutex<InsertOnlyRingBuffer<T>>,
    /// The maximum time to wait for acquiring the lock before timing out.
    max_delay_ms: Duration,
    /// Count of failed attempts to acquire the lock, for bookkeeping purposes.
    lock_failures: AtomicU64,
}

impl fmt::Display for DebugStoreStorageError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "Failed to acquire lock within the specified timeout")
    }
}

impl Error for DebugStoreStorageError {}

impl<T: Clone> DebugStoreStorage<T> {
    /// Creates a new DebugStoreStorage with the specified size and maximum delay.
    pub fn new(size: usize, max_delay_ms: u64) -> Self {
        Self {
            buffer: Mutex::new(InsertOnlyRingBuffer::new(size)),
            max_delay_ms: Duration::from_millis(max_delay_ms),
            lock_failures: AtomicU64::new(0),
        }
    }

    /// Inserts a value into the storage, returning an error if the lock cannot be acquired.
    pub fn insert(&self, value: T) -> Result<(), DebugStoreStorageError> {
        if let Some(mut buffer) = self.buffer.try_lock_for(self.max_delay_ms) {
            buffer.insert(value);
            Ok(())
        } else {
            self.lock_failures.fetch_add(1, Ordering::Relaxed);
            Err(DebugStoreStorageError::LockTimeout)
        }
    }

    /// Folds over the elements in the storage using the provided function.
    pub fn fold<U, F>(&self, init: U, mut func: F) -> Option<U>
    where
        F: FnMut(U, &T) -> U,
    {
        self.buffer
            .try_lock_for(self.max_delay_ms)
            // Copy the buffer, releasing the lock
            .map(|buffer| buffer.to_vec_ordered())
            // Perform the fold operation on the cloned data
            .map(|local_storage| local_storage.into_iter().fold(init, |acc, item| func(acc, &item)))
    }

    /// Returns the number of times the lock was not acquired immediately.
    pub fn get_lock_misses(&self) -> u64 {
        self.lock_failures.load(Ordering::Relaxed)
    }

    /// Returns the number of elements that have been inserted into the storage.
    pub fn len(&self) -> Result<usize, DebugStoreStorageError> {
        self.buffer
            .try_lock_for(self.max_delay_ms)
            .map(|buffer| buffer.len())
            .ok_or(DebugStoreStorageError::LockTimeout)
    }
}

/// A ring buffer that supports insertions and maintains a fixed capacity.
pub struct InsertOnlyRingBuffer<T> {
    storage: Vec<Option<T>>,
    capacity: usize,
    head: usize,
}

impl<T: Clone> InsertOnlyRingBuffer<T> {
    pub fn new(capacity: usize) -> Self {
        Self { storage: vec![None; capacity], capacity, head: 0 }
    }

    /// Inserts a value into the ring buffer, overwriting the oldest value if the buffer is full.
    pub fn insert(&mut self, value: T) {
        if self.capacity == 0 {
            return;
        }
        self.storage[self.head] = Some(value);
        self.head = (self.head + 1) % self.capacity;
    }

    /// Checks if the ring buffer is full.
    pub fn is_full(&self) -> bool {
        self.capacity == 0 || self.storage[self.capacity - 1].is_some()
    }

    /// Returns the number of elements in the ring buffer.
    pub fn len(&self) -> usize {
        if self.is_full() {
            self.capacity
        } else {
            self.head
        }
    }

    /// Converts the elements of the ring buffer to a Vec in the order they were inserted.
    pub fn to_vec_ordered(&self) -> Vec<T> {
        self.storage
            .iter()
            .cycle()
            .skip(self.head)
            .take(self.capacity)
            .filter_map(|opt| opt.as_ref().cloned())
            .collect()
    }
}
