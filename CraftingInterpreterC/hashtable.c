#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "hashtable.h"
#include "value.h"

// Chapter 20

#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

// string intern 
void debugPrintTable(Table* table, char* description, bool isStringIntern) {
    printf("** Dumping contents of table %s\n", description);
    printf("Count %d capacity %d\n", table->count, table->capacity);
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL) {
            if (isStringIntern) {
                // see ch 20.5 page 327
                printf("String intern entry at slot %i has unique string %.*s.", i, entry->key->length, entry->key->chars);
            }
            else {
                printf("Entry at slot %i has key %.*s.  Value Type:", i, entry->key->length, entry->key->chars);
                printValueType(entry->value);
                printf(" Value: ");
                printValue(entry->value);
            }
            
            printf("\n");
        }
    }
}

void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}
// NOTE: The "Optimization" chapter has a manual copy of this function.
// If you change it here, make sure to update that copy.


static Entry* findEntry(Entry* entries, int capacity,
    ObjString* key) {

    //> Optimization initial-index
    uint32_t index = key->hash & (capacity - 1);

    Entry* tombstone = NULL;

    // we have to find a slot, since we resize before the hash table gets full
    for (;;) {
        Entry* entry = &entries[index];
        /* Hash Tables find-entry < Hash Tables find-tombstone
            if (entry->key == key || entry->key == NULL) {
              return entry;
            }
        */

        if (entry->key == NULL) {
            if (IS_NIL(entry->value)) {
                // Empty entry.
                return tombstone != NULL ? tombstone : entry;
            }
            else {
                // We found a tombstone.
                if (tombstone == NULL) tombstone = entry;
            }
        }
        else if (entry->key == key) {
            // We found the key.
            return entry;
        }

        /* Hash Tables find-entry < Optimization next-index
            index = (index + 1) % capacity;
        */

        index = (index + 1) & (capacity - 1);
    }
}

bool tableGet(Table* table, ObjString* key, Value* value) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    *value = entry->value;
    return true;
}

static void adjustCapacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NULL;
        entries[i].value = NIL_VAL;
    }

    table->count = 0;

    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key == NULL) continue;

        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(Table* table, ObjString* key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }

    Entry* entry = findEntry(table->entries, table->capacity, key);
    bool isNewKey = entry->key == NULL;

    // NIL is an empty slot; otherwise this is a Tombstone and with BOOL_VAL
    // If we are replacing a Tombstone with a valid entry, the count wont increase
    // See ch 20.4.5 pg 376
    if (isNewKey && IS_NIL(entry->value)) table->count++; 

    entry->key = key;
    entry->value = value;
    return isNewKey;
}

bool tableDelete(Table* table, ObjString* key) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->key == NULL) return false;

    entry->key = NULL;
    entry->value = BOOL_VAL(true);
    return true;
}

void tableAddAll(Table* from, Table* to) {
    for (int i = 0; i < from->capacity; i++) {
        Entry* entry = &from->entries[i];
        if (entry->key != NULL) {
            tableSet(to, entry->key, entry->value);
        }
    }
}

ObjString* tableFindString(Table* table, const char* chars,
    int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    // uint32_t index = hash % table->capacity; // version as of Ch 20.5 pg 379
    //> Optimization find-string-index
    uint32_t index = hash & (table->capacity - 1);
    //< Optimization find-string-index
    
    for (;;) {
        Entry* entry = &table->entries[index];
        if (entry->key == NULL) {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value)) return NULL;
        }
        else if (entry->key->length == length &&
            entry->key->hash == hash &&
            memcmp(entry->key->chars, chars, length) == 0) {
            // We found it.
            return entry->key;
        }

        // index = (index + 1) % table->capacity; // version as of Ch 20.5 pg 379
        //> Optimization find-string-next
        index = (index + 1) & (table->capacity - 1);
        //< Optimization find-string-next
    }
}

void tableRemoveWhite(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->key != NULL && !entry->key->obj.isMarked) {
            tableDelete(table, entry->key);
        }
    }
}
/* LLM later for GC
void markTable(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markObject((Obj*)entry->key);
        markValue(entry->value);
    }
}
*/