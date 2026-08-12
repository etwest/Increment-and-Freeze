#include "iaf_api.h"
#include <iostream>

extern "C" {
int main() {
  Iaf h = nullptr;
  h = Iaf_create(0, 1000);
 
  std::cout << "id: " << Iaf_grab_id(h) << std::endl;

  Iaf_write(h, (void *)Iaf_grab_id(h), 512);
  Iaf_write(h, (void *)Iaf_grab_id(h), 256);

  Iaf_write(h, (void *)Iaf_grab_id(h), 512);
  Iaf_write(h, (void *)-1, 256);
  Iaf_write(h, (void *)(Iaf_grab_id(h)-2), 512);

  std::cout << Iaf_stringify(h) << std::endl;

  Iaf_destroy(&h);

  std::cout << "h: " << h << std::endl;
}
}