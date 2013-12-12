#inclue "obj/hp_obj.h"

unsigned
hp_obj_validate(struct hp_obj *obj, unsigned type)
{
  return (obj->type == type);
}
