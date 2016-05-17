#include "ThreadStorage.h"
#include <memory>
#include <unordered_map>

using namespace std;

namespace {

    class Storage {
    public:
        static Storage* getStorage() {
            static thread_local Storage instance;
            if (instance.destroyed) {
                return nullptr;
            }
            return &instance;
        }

        ItemBase* get_item(const ThreadStorageBase* key) {
            auto it = storage.find(key);
            if (it != storage.end()) {
                return it->second.get();
            }
            return nullptr;
        }

        void insert_item(const ThreadStorageBase* key, ItemBase* item) {
            storage[key].reset(item);
        }

    private:
        bool destroyed;
        Storage() : destroyed(false) {}
        ~Storage() { destroyed = true; }
        unordered_map<const ThreadStorageBase*, unique_ptr<ItemBase>> storage;
    };


}

ItemBase::~ItemBase() {}

ThreadStorageBase::ThreadStorageBase() {}
ThreadStorageBase::~ThreadStorageBase() {}

bool ThreadStorageBase::hasLocalData() const
{
    Storage* s = Storage::getStorage();
    if (!s) {
        return false;
    }
    return s->get_item(this) != nullptr;
}

ItemBase* ThreadStorageBase::get_item()
{
    Storage* s = Storage::getStorage();
    if (!s) {
        return nullptr;
    }
    return s->get_item(this);
}

void ThreadStorageBase::set_item(ItemBase* item)
{
    Storage* s = Storage::getStorage();
    if (s) {
        s->insert_item(this, item);
    } else {
        // fail silently, because we are shutting down
        delete item;
    }
}


void initialize_thread_storage()
{
    Storage::getStorage();
}
