#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

// hash table maximum load factor.
#define TABLE_MAX_LOAD 0.75

void initTable(Table *table) {
	table->count = 0;
	table->capacity = 0;
	table->entries = NULL;
}

void freeTable(Table *table) {
	FREE_ARRAY(table->entries, Entry, table->capacity);
	initTable(table);
}

static Entry *findEntry(Entry *entries, int capacity, ObjString *key) {
	uint32_t index = key->hash % capacity;
	for (;;) {
		Entry *entry = &entries[index];

		if (entry->key == key || !entry->key) {
			return entry;
		}

		index = (index + 1) % capacity;
	}
}

static void adjustCapacity(Table *table, int new_capacity) {
	Entry *entries = ALLOCATE(Entry, new_capacity);

	for (int i = 0; i < table->capacity; ++i) {
		Entry *entry = &table->entries[i];

		if (!entry->key) {
			entries[i].key = NULL;
			entries[i].value = NIL_VAL;
		} else {
			Entry *new_entry = findEntry(entries, new_capacity, entry->key);
			new_entry->key = entry->key;
			new_entry->value = entry->value;
		}
	}

	// initialize the rest of the entries.
	for (int i = table->capacity; i < new_capacity; ++i) {
		entries[i].key = NULL;
		entries[i].value = NIL_VAL;
	}

	FREE_ARRAY(table->entries, Entry, table->capacity);
	table->entries = entries;
	table->capacity = new_capacity;
}

bool tableSet(Table *table, ObjString *key, Value value) {
	if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
		int capacity = GROW_CAPACITY(table->capacity);
		adjustCapacity(table, capacity);
	}

	Entry *entry = findEntry(table->entries, table->capacity, key);
	bool is_new_entry = entry->key == NULL;
	entry->key = key;
	entry->value = value;

	if (is_new_entry) ++table->count;

	return is_new_entry;
}