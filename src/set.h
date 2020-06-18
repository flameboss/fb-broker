
/*
 * Set of strings
 */

void* set_create(void);

void set_add(void* set, char *e);

int set_exists(void* set, char *e);

void set_destroy(void *set);
