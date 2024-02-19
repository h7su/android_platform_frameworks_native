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

use crossbeam_queue::ArrayQueue;

/// A thread-safe storage that allows non-blocking attempts to store and visit elements.
pub struct Storage<T, const N: usize> {
    insertion_buffer: ArrayQueue<T>,
}

impl<T, const N: usize> Storage<T, N> {
    /// Creates a new Storage with the specified size.
    pub fn new() -> Self {
        Self { insertion_buffer: ArrayQueue::new(N) }
    }

    /// Inserts a value into the storage, returning an error if the lock cannot be acquired.
    pub fn insert(&self, value: T) {
        self.insertion_buffer.force_push(value);
    }

    /// Folds over the elements in the storage using the provided function.
    pub fn fold<U, F>(&self, init: U, mut func: F) -> U
    where
        F: FnMut(U, &T) -> U,
    {
        let mut acc = init;
        while let Some(value) = self.insertion_buffer.pop() {
            acc = func(acc, &value);
        }
        acc
    }

    /// Returns the number of elements that have been inserted into the storage.
    pub fn len(&self) -> usize {
        self.insertion_buffer.len()
    }
}
