
#include <map>

typedef std::map<char *, bool> Map;

extern "C" {

void* set_create() {
  return reinterpret_cast<void*> (new Map);
}

void set_add(void *map, char *k) {
  Map* m = reinterpret_cast<Map*> (map);
  m->insert(std::pair<char *, bool>(k, true));
}

int set_exists(void *map, char *k) {
  Map* m = reinterpret_cast<Map*> (map);
  if (m->find(k) == m->end()) {
      return 0;
  }
  else {
      return 1;
  }
}

void set_destroy(void *map) {
    Map* m = reinterpret_cast<Map*> (map);
    delete m;
}

// etc...

} // extern "C"
