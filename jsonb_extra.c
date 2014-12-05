#include "postgres.h"
#include <string.h>
#include "fmgr.h"
#include "funcapi.h"
#include "utils/array.h"
#include "catalog/pg_type.h"

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#include "utils/jsonb.h"
#include "utils/builtins.h"

typedef struct JbqResult
{
  Jsonb **result;
  int  size;
  int count;
} JbqResult;

char *JsbvToStr(JsonbValue *v);
char *JbToStr(Jsonb *v);
void debugJsonb(JsonbValue *jb);
void walk(JbqResult *result, JsonbValue *jb, Datum *path, int level, int size);
bool isArray(JsonbValue *v);
void addJbqResult(JbqResult *result, JsonbValue *val);

JsonbValue toJsonbString(Datum str);
/* static void recursiveAny(JsonbValue *jb); */

PG_FUNCTION_INFO_V1(jsonb_extract);

char *JsbvToStr(JsonbValue *v){
  Jsonb	*j;
  j = JsonbValueToJsonb(v);
  return JsonbToCString(NULL, &j->root, VARSIZE(j));
}

char *JbToStr(Jsonb *v){
  return JsonbToCString(NULL, &v->root, VARSIZE(v));
}

void addJbqResult(JbqResult *result, JsonbValue *val){
  if (result->count >= result->size){
    result->size *= 2;
    result->result = repalloc(result->result, sizeof(Jsonb *) * result->size);
  }
  /* elog(DEBUG1, "ADDDDD: %s", JsbvToStr(val)); */
  /* elog(DEBUG1, "ADDDDD: %s", JbToStr(JsonbValueToJsonb(val))); */
  result->result[result->count++] = JsonbValueToJsonb(val);
}

bool isArray(JsonbValue *v){
  JsonbValue	tv;
  if (v->type == jbvBinary)
  {
    JsonbIterator *it = JsonbIteratorInit((JsonbContainer *) v->val.binary.data);
    return JsonbIteratorNext(&it, &tv, true) == WJB_BEGIN_ARRAY;
  } else {
    return false;
  }
}

  Datum
jsonb_extract(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB(0);
  ArrayType  *path = PG_GETARG_ARRAYTYPE_P(1);

  Datum	  *pathtext;
  bool	  *pathnulls;
  int 		npath;
  int i;

  JsonbValue	jbv;
  JbqResult *result;
  ArrayType  *result_array;

  result = palloc(sizeof(JbqResult));
  result->size = 256;
  result->count = 0;
  result->result = palloc(256 * sizeof(JsonbValue *));

  deconstruct_array(path, TEXTOID, -1, false, 'i', &pathtext, &pathnulls, &npath);
  /* elog(NOTICE, "npath %i", npath); */

  jbv.type = jbvBinary;
  jbv.val.binary.data = &jb->root;
  jbv.val.binary.len = VARSIZE_ANY_EXHDR(jb);

  walk(result, &jbv, pathtext, 0, npath);

  elog(DEBUG1, "Result count: %i", result->count);

  if(result->count > 0){
    result_array = construct_array((Datum *) result->result, result->count, JSONBOID, -1, false, 'i');

    for (i = 0; i < result->count; i++){
      elog(DEBUG1, "* %s", JbToStr(result->result[i]));
      pfree(result->result[i]);
    }
    pfree(result->result);
    pfree(result);
  }

  if(result->count > 0){
    PG_RETURN_POINTER(result_array);
  }else{
    PG_RETURN_NULL();
  }
}

JsonbValue toJsonbString(Datum str){
  JsonbValue	key;
  key.type = jbvString;
  key.val.string.val = VARDATA_ANY(str);
  key.val.string.len = VARSIZE_ANY_EXHDR(str);
  return key;
}

void walk(JbqResult *result, JsonbValue *jb, Datum *path, int level, int size){
  JsonbValue	key;
  JsonbValue *next_value;
  JsonbIterator *array_it;
  JsonbValue	array_value;
  int next_it;


  elog(DEBUG1,"%i %i", level, size);
  /* debugJsonb(jb); */

  if (level == size){
    addJbqResult(result, jb);
    return;
  }else{
    elog(DEBUG1, "take %s of %s", TextDatumGetCString(path[level]), JsbvToStr(jb));
  }

  if(jb->type == jbvBinary){
    /* elog(DEBUG1,"enter jbvBinary"); */

    key = toJsonbString(path[level]);
    next_value = findJsonbValueFromContainer(
        (JsonbContainer *) jb->val.binary.data,
        JB_FOBJECT, &key);

    if(next_value == NULL){
      return;
    }

    if(next_value->type == jbvBinary){
      /* elog(DEBUG1,"next value"); */

      array_it = JsonbIteratorInit((JsonbContainer *) next_value->val.binary.data);
      /* elog(DEBUG1,"get inter"); */

      next_it = JsonbIteratorNext(&array_it, &array_value, true);
      if(next_it == WJB_BEGIN_ARRAY){
        /* elog(DEBUG1, "WJB_BEGIN_ARRAY"); */
        while ((next_it = JsonbIteratorNext(&array_it, &array_value, true)) != WJB_DONE){
          if(next_it == WJB_ELEM){
            debugJsonb(&array_value);
            walk(result, &array_value, path, level + 1, size);
          }
        }
      }
      else if(next_it == WJB_BEGIN_OBJECT){
        /* elog(DEBUG1, "WJB_BEGIN_OBJECT"); */
        walk(result, next_value, path, level + 1, size);
      }
    }else{
      walk(result, next_value, path, level + 1, size);
    }
  }
}


/* Datum */
/* nicola(PG_FUNCTION_ARGS) */
/* { */
/*   Jsonb *jb = PG_GETARG_JSONB(0); */
/*   ArrayType  *path = PG_GETARG_ARRAYTYPE_P(1); */

/*   Datum	  *pathtext; */
/*   bool	  *pathnulls; */
/*   int 		npath; */
/*   int     i; */
/*   JsonbValue *jbvp = NULL; */
/*   JsonbValue	key; */
/* 	JsonbContainer *container; */
/*   int reslen = 0; */

/*   deconstruct_array(path, TEXTOID, -1, false, 'i', &pathtext, &pathnulls, &npath); */
/*   elog(NOTICE, "npath %i", npath); */

/* 	container = &jb->root; */
/*   for (i = 0; i < npath; i++) { */

/*     key.type = jbvString; */
/*     key.val.string.val = VARDATA_ANY(pathtext[i]); */
/*     key.val.string.len = VARSIZE_ANY_EXHDR(pathtext[i]); */

/*     jbvp = findJsonbValueFromContainer(container, JB_FOBJECT, &key); */
/*     elog(NOTICE,"key %s: step %s, %i", */
/*         TextDatumGetCString(pathtext[i]), */
/*         JsbvToStr(jbvp), */
/*         jbvp->type); */
/*     if(jbvp->type == jbvBinary){ */
/*       container = (JsonbContainer *) jbvp->val.binary.data; */
/*     }else{ */
/*       PG_RETURN_NULL(); */
/*     } */
/*   } */

/*   if(jbvp == NULL){ */
/*     PG_RETURN_NULL(); */
/*   }else{ */
/*     PG_RETURN_JSONB(JsonbValueToJsonb(jbvp)); */
/*   } */
/* } */


void debugJsonb(JsonbValue *jb){
  switch (jb->type)
  {
    case jbvNull:
      elog(DEBUG1, "D:jbvNull");
      break;
    case jbvString:
      elog(DEBUG1, "D:jbvString, %s", JsbvToStr(jb));
      break;
    case jbvNumeric:
      elog(DEBUG1, "D:jbvNumeric, %s", JsbvToStr(jb));
      break;
    case jbvBool:
      elog(DEBUG1, "D:jbvBool");
      break;
    case jbvArray:
      elog(DEBUG1, "D:jbvArray");
      break;
    case jbvObject:
      elog(DEBUG1, "D:jbvObject");
      break;
    case jbvBinary:
      elog(DEBUG1, "D:jbvBinary");
      break;
    default:
      elog(DEBUG1, "D:other");
  }
}

/* static void */
/* old_recursiveAny(JsonbValue *jb) { */
/*   JsonbIterator	*it; */
/*   int32			 r; */
/*   JsonbValue v; */

/*   elog(DEBUG1, "ITERATE %s",JsbvToStr(jb)); */

/*   it = JsonbIteratorInit(jb->val.binary.data); */
/*   while((r = JsonbIteratorNext(&it, &v, false)) != WJB_DONE) */
/*   { */
/*     if (r == WJB_KEY) */
/*     { */
/*       elog(DEBUG1, "key: %s", JsbvToStr(&v)); */
/*       r = JsonbIteratorNext(&it, &v, false); */
/*       Assert(r == WJB_VALUE); */
/*     } */
/*     if (r == WJB_VALUE || r == WJB_ELEM) */
/*     { */
/*       elog(DEBUG1,"  value type: %i, %s", v.type, JsbvToStr(&v)); */
/*       switch (v.type) */
/*       { */
/*         case jbvNull: */
/*           elog(DEBUG1, "jbvNull"); */
/*           break; */
/*         case jbvString: */
/*           elog(DEBUG1, "jbvString, %s", JsbvToStr(&v)); */
/*           break; */
/*         case jbvNumeric: */
/*           elog(DEBUG1, "jbvNumeric, %s", JsbvToStr(&v)); */
/*           break; */
/*         case jbvBool: */
/*           elog(DEBUG1, "jbvBool"); */
/*           break; */
/*         case jbvArray: */
/*           elog(DEBUG1, "jbvArray"); */
/*           /1* recursiveAny(&v); *1/ */
/*           break; */
/*         case jbvObject: */
/*           elog(DEBUG1, "jbvObject"); */
/*           /1* recursiveAny(&v); *1/ */
/*           break; */
/*         case jbvBinary: */
/*           elog(DEBUG1, "jbvBinary"); */
/*           /1* recursiveAny(&v); *1/ */
/*           break; */
/*         default: */
/*           elog(DEBUG1, "other"); */
/*       } */
/*       /1* recursiveAny(&v); *1/ */
/*       /1* if (v.type == jbvBinary) *1/ */
/*       /1* 	recursiveAny(&v); *1/ */
/*     } */
/*   } */
/* } */

/* text  *key = PG_GETARG_TEXT_P(1); */
/* JsonbValue	k; */
/* uint32 keylen; */

/* container = &jb->root; */

/* keylen = VARSIZE_ANY_EXHDR(key); */
/* k.type = jbvString; */
/* k.val.string.val = text_to_cstring(key); */
/* k.val.string.len = keylen; */

/* elog(DEBUG1, "key is %s, %i", text_to_cstring(key), keylen); */

/* jbvp = findJsonbValueFromContainer(container, */
/* JB_FOBJECT, &k); */

/* elog(DEBUG1, "Get value"); */

/* if (jbvp == NULL) */
/* PG_RETURN_NULL(); */

/* res = JsonbValueToJsonb(jbvp); */

/* if (res == NULL) */
/* PG_RETURN_NULL(); */
/* else */
/* PG_RETURN_JSONB(res); */
