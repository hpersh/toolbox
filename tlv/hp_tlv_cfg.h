#ifndef __HP_TLV_CFG_H
#define __HP_TLV_CFG_H

/* Base types for TLV integer and float types */
typedef int    hp_tlv_intval;
typedef double hp_tlv_floatval;

/* Format strings for TLV integer and float types */
#define HP_TLV_FMT_INT    "%d"
#define HP_TLV_FMT_FLOAT  "%lg"

/* Stream formatting parameters */
enum {
  HP_TLV_HDR_TYPE_BITS = 4,	/* TLV header type field length, in bits */
  HP_TLV_HDR_LEN_BITS  = 8	/* TLV length type field length, in bits */
};

#endif /* __HP_TLV_CFG_H */
