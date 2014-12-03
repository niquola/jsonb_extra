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
	JsonbValue	  **result;
	int			size;
	int			count;
} JbqResult;

char *toStr(JsonbValue *v);
void debugJsonb(JsonbValue *jb);
char *toStr(JsonbValue *v);
void walk(JbqResult *result, JsonbValue *jb, Datum *path, int level, int size);
bool isArray(JsonbValue *v);
JsonbValue toJsonbString(Datum str);
/* static void recursiveAny(JsonbValue *jb); */

PG_FUNCTION_INFO_V1(jsonb_extract);

char *toStr(JsonbValue *v){
  Jsonb	*j;
  j = JsonbValueToJsonb(v);
  return JsonbToCString(NULL, &j->root, VARSIZE(j));
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

  elog(INFO, "Result count: %i", result->count);

	for (i = 0; i < result->count; i++){
    elog(INFO, "* %s", toStr(result->result[i]));
  }
	/* for (i = 0; i < state->result_count; i++) */
	/* 	pfree(state->result[i]); */
	/* pfree(state->result); */
	/* pfree(state); */

  PG_RETURN_NULL();
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


  elog(INFO,"%i %i", level, size);
  /* debugJsonb(jb); */

  if (level == size){
    if (result->count >= result->size){
      result->size *= 2;
      result->result = repalloc(result->result, sizeof(JsonbValue *) * result->size);
    }
    result->result[result->count++] = jb;
    return;
  }else{
    elog(NOTICE, "take %s of %s", TextDatumGetCString(path[level]), toStr(jb));
  }

  if(jb->type == jbvBinary){
    /* elog(INFO,"enter jbvBinary"); */

    key = toJsonbString(path[level]);
    next_value = findJsonbValueFromContainer(
        (JsonbContainer *) jb->val.binary.data,
        JB_FOBJECT, &key);

    if(next_value == NULL){
      return;
    }

    if(next_value->type == jbvBinary){
      /* elog(INFO,"next value"); */

      array_it = JsonbIteratorInit((JsonbContainer *) next_value->val.binary.data);
      /* elog(INFO,"get inter"); */

      next_it = JsonbIteratorNext(&array_it, &array_value, true);
      if(next_it == WJB_BEGIN_ARRAY){
        /* elog(INFO, "WJB_BEGIN_ARRAY"); */
        while ((next_it = JsonbIteratorNext(&array_it, &array_value, true)) != WJB_DONE){
          if(next_it == WJB_ELEM){
            debugJsonb(&array_value);
            walk(result, &array_value, path, level + 1, size);
          }
        }
      }
      else if(next_it == WJB_BEGIN_OBJECT){
        /* elog(INFO, "WJB_BEGIN_OBJECT"); */
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
/*         toStr(jbvp), */
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
      elog(INFO, "D:jbvNull");
      break;
    case jbvString:
      elog(INFO, "D:jbvString, %s", toStr(jb));
      break;
    case jbvNumeric:
      elog(INFO, "D:jbvNumeric, %s", toStr(jb));
      break;
    case jbvBool:
      elog(INFO, "D:jbvBool");
      break;
    case jbvArray:
      elog(INFO, "D:jbvArray");
      break;
    case jbvObject:
      elog(INFO, "D:jbvObject");
      break;
    case jbvBinary:
      elog(INFO, "D:jbvBinary");
      break;
    default:
      elog(INFO, "D:other");
  }
}

/* static void */
/* old_recursiveAny(JsonbValue *jb) { */
/*   JsonbIterator	*it; */
/*   int32			 r; */
/*   JsonbValue v; */

/*   elog(INFO, "ITERATE %s",toStr(jb)); */

/*   it = JsonbIteratorInit(jb->val.binary.data); */
/*   while((r = JsonbIteratorNext(&it, &v, false)) != WJB_DONE) */
/*   { */
/*     if (r == WJB_KEY) */
/*     { */
/*       elog(INFO, "key: %s", toStr(&v)); */
/*       r = JsonbIteratorNext(&it, &v, false); */
/*       Assert(r == WJB_VALUE); */
/*     } */
/*     if (r == WJB_VALUE || r == WJB_ELEM) */
/*     { */
/*       elog(INFO,"  value type: %i, %s", v.type, toStr(&v)); */
/*       switch (v.type) */
/*       { */
/*         case jbvNull: */
/*           elog(INFO, "jbvNull"); */
/*           break; */
/*         case jbvString: */
/*           elog(INFO, "jbvString, %s", toStr(&v)); */
/*           break; */
/*         case jbvNumeric: */
/*           elog(INFO, "jbvNumeric, %s", toStr(&v)); */
/*           break; */
/*         case jbvBool: */
/*           elog(INFO, "jbvBool"); */
/*           break; */
/*         case jbvArray: */
/*           elog(INFO, "jbvArray"); */
/*           /1* recursiveAny(&v); *1/ */
/*           break; */
/*         case jbvObject: */
/*           elog(INFO, "jbvObject"); */
/*           /1* recursiveAny(&v); *1/ */
/*           break; */
/*         case jbvBinary: */
/*           elog(INFO, "jbvBinary"); */
/*           /1* recursiveAny(&v); *1/ */
/*           break; */
/*         default: */
/*           elog(INFO, "other"); */
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

/* elog(INFO, "key is %s, %i", text_to_cstring(key), keylen); */

/* jbvp = findJsonbValueFromContainer(container, */
/* JB_FOBJECT, &k); */

/* elog(INFO, "Get value"); */

/* if (jbvp == NULL) */
/* PG_RETURN_NULL(); */

/* res = JsonbValueToJsonb(jbvp); */

/* if (res == NULL) */
/* PG_RETURN_NULL(); */
/* else */
/* PG_RETURN_JSONB(res); */
