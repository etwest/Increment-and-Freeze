// C-compatible header

#ifdef __cplusplus
extern "C" {
#endif

struct Iaf_t;
typedef struct Iaf_t* Iaf;

Iaf Iaf_create();
void Iaf_write(Iaf h, void* addr);
void Iaf_print(Iaf h);
void Iaf_destroy(Iaf h);

#ifdef __cplusplus
}
#endif