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

typedef struct JbvResult
{
  JsonbValue **result;
  int  size;
  int count;
} JbvResult;

char *JsbvToStr(JsonbValue *v);
void debugJsonb(JsonbValue *jb);
void walk(JbvResult *result, JsonbValue *jb, Datum *path, int level, int size);
bool isArray(JsonbValue *v);
JsonbValue *JsonbToJsonbValue(Jsonb *v);
void addToJbvResult(JbvResult *result, JsonbValue *val);
JbvResult *jsonb_extract_internal(Jsonb *jb, ArrayType *path);
text *JsonbValueToText(JsonbValue *v);
JsonbValue *pushJsonbToParseState(JsonbParseState **pstate, Jsonb *jb);

JsonbValue toJsonbString(Datum str);
/* static void recursiveAny(JsonbValue *jb); */

PG_FUNCTION_INFO_V1(jsonb_extract);
PG_FUNCTION_INFO_V1(jsonb_extract_text);
PG_FUNCTION_INFO_V1(jsonb_as_text);
PG_FUNCTION_INFO_V1(jsonb_update);

char *JsbvToStr(JsonbValue *v){
  Jsonb	*j;
  j = JsonbValueToJsonb(v);
  return JsonbToCString(NULL, &j->root, VARSIZE(j));
}

void addToJbvResult(JbvResult *result, JsonbValue *val){
  if (result->count >= result->size){
    result->size *= 2;
    result->result = repalloc(result->result, sizeof(JsonbValue *) * result->size);
  }
  result->result[result->count++] = val;
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

/* TODO: function returns address of local variable */
JsonbValue *
JsonbToJsonbValue(Jsonb *v){
  JsonbValue jv;
  int r;

  JsonbIterator *it = JsonbIteratorInit(&v->root);

  //this is problem because i return local variable address
  //but if i switch to JsonbValue *v and call JsonbIteratorNext it fails
  //should i palloc it?
  while ((r = JsonbIteratorNext(&it, &jv, true)) != WJB_DONE){
    if (r == WJB_ELEM){
      return &jv;
    }
  }
  return NULL;
}

JsonbValue *pushJsonbToParseState(JsonbParseState **pstate, Jsonb *jb)
{
  JsonbIterator *it;
  JsonbValue v;
  int type;
  JsonbValue *res = NULL;

  it = JsonbIteratorInit(&jb->root);
  while ((type = JsonbIteratorNext(&it, &v, false)) != WJB_DONE)
  {
    res = pushJsonbValue(pstate, type, &v);
  }
  return res;
}

/* TODO: create path if not exists */
/* TODO: support arrays */
Datum
jsonb_update(PG_FUNCTION_ARGS)
{
  Jsonb         *jb = PG_GETARG_JSONB(0);
  ArrayType   *path = PG_GETARG_ARRAYTYPE_P(1);
  Jsonb      *jbval = PG_GETARG_JSONB(2);
  int type;
  JsonbValue v;
  char *keystr;
  JsonbValue *res = NULL;
  JsonbParseState *pstate = NULL;

  Datum	  *pathtext;
  bool	  *pathnulls;
  int    	   npath;
  int             path_index = 0;
  int             level = -1;
  bool            matched = false;
  JsonbIterator *it;

  deconstruct_array(path, TEXTOID, -1, false, 'i', &pathtext, &pathnulls, &npath);

  it = JsonbIteratorInit(&jb->root);

  while ((type = JsonbIteratorNext(&it, &v, false)) != WJB_DONE)
  {
    switch (type)
    {
      case WJB_KEY:
        keystr = TextDatumGetCString(pathtext[path_index]);
        if(strcmp(keystr, v.val.string.val) == 0)
        {
          if(level == path_index && (npath - 1) == path_index){
            matched = true;
          }else{
            path_index++;
            matched = false;
          }
        }else{
          res = pushJsonbValue(&pstate, WJB_KEY, &v);
          matched = false;
        }
        res = pushJsonbValue(&pstate, WJB_KEY, &v);
        break;
      case WJB_VALUE:
        if(matched){
          res = pushJsonbToParseState(&pstate, jbval);
        }else{
          res = pushJsonbValue(&pstate, WJB_VALUE, &v);
        }
        break;
      case WJB_ELEM:
        if(matched){
          res = pushJsonbToParseState(&pstate, jbval);
        }else{
          res = pushJsonbValue(&pstate, WJB_ELEM, &v);
        }
        break;
      case WJB_BEGIN_ARRAY:
        res = pushJsonbValue(&pstate, WJB_BEGIN_ARRAY, &v);
        break;
      case WJB_END_ARRAY:
        res = pushJsonbValue(&pstate, WJB_END_ARRAY, NULL);
        break;
      case WJB_BEGIN_OBJECT:
        level++;
        res = pushJsonbValue(&pstate, WJB_BEGIN_OBJECT, &v);
        break;
      case WJB_END_OBJECT:
        level--;
        res = pushJsonbValue(&pstate, WJB_END_OBJECT, &v);
        matched = false;
        break;
      default:
        elog(INFO, "UPS");
    }

  }
  if(res == NULL){
    PG_RETURN_NULL();
  }else{
    PG_RETURN_JSONB(JsonbValueToJsonb(res));
  }

}

  Datum
jsonb_as_text(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB(0);
  JsonbValue *jv = JsonbToJsonbValue(jb);
  if(jv == NULL){
    PG_RETURN_NULL();
  }else{
    PG_RETURN_TEXT_P(JsonbValueToText(jv));
  }
}

  Datum
jsonb_extract(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB(0);
  ArrayType  *path = PG_GETARG_ARRAYTYPE_P(1);

  JbvResult *result;
  Jsonb **jsonbs;
  ArrayType  *result_array;
  int i;

  result = jsonb_extract_internal(jb, path);
  if (result->count == 0) {
    PG_RETURN_NULL();
  }else{
    jsonbs = palloc(result->count * sizeof(Jsonb *));
    for(i=0; i< result->count; i++){
      /* elog(INFO, "%s", JsbvToStr(result->result[i])); */
      jsonbs[i] = JsonbValueToJsonb(result->result[i]);
    }
    result_array = construct_array((Datum *) jsonbs, result->count, JSONBOID, -1, false, 'i');

    for(i=0; i< result->count; i++)
      pfree(jsonbs[i]);

    pfree(jsonbs);
    PG_RETURN_POINTER(result_array);
  }

}

  Datum
jsonb_extract_text(PG_FUNCTION_ARGS)
{
  Jsonb *jb = PG_GETARG_JSONB(0);
  ArrayType  *path = PG_GETARG_ARRAYTYPE_P(1);

  JbvResult  *result;
  text **texts;
  ArrayType *result_texts;

  int i;
  /* ArrayType  *texts; */

  result = jsonb_extract_internal(jb, path);
  if (result->count == 0) {
    PG_RETURN_NULL();
  }else{
    texts = palloc(result->count * sizeof(text));
    for(i=0; i< result->count; i++){

      /* elog(INFO, "%s", JsbvToStr(result->result[i])); */
      texts[i] = JsonbValueToText(result->result[i]);
    }
    result_texts = construct_array((Datum *) texts, result->count, TEXTOID, -1, false, 'i');
    for(i=0; i< result->count; i++)
      pfree(texts[i]);

    pfree(texts);
    PG_RETURN_POINTER(result_texts);
  }
}


  JbvResult
*jsonb_extract_internal(Jsonb *jb, ArrayType *path)
{
  Datum	  *pathtext;
  bool	  *pathnulls;
  int 		npath;

  JsonbValue	jbv;
  JbvResult *result;

  result = palloc(sizeof(JbvResult));
  result->size = 256;
  result->count = 0;
  result->result = palloc(256 * sizeof(JsonbValue *));

  deconstruct_array(path, TEXTOID, -1, false, 'i', &pathtext, &pathnulls, &npath);
  /* elog(NOTICE, "npath %i", npath); */

  jbv.type = jbvBinary;
  jbv.val.binary.data = &jb->root;
  jbv.val.binary.len = VARSIZE_ANY_EXHDR(jb);

  walk(result, &jbv, pathtext, 0, npath);

  /* elog(DEBUG1, "Result count: %i", result->count); */
  return result;
}

text *JsonbValueToText(JsonbValue *v){
  switch(v->type)
  {
    case jbvNull:
      return cstring_to_text(""); // better pg null?
      break;
    case jbvBool:
      return cstring_to_text(v->val.boolean ? "true" : "false");
      break;
    case jbvString:
      return cstring_to_text_with_len(v->val.string.val, v->val.string.len);
      break;
    case jbvNumeric:
      return cstring_to_text(DatumGetCString(DirectFunctionCall1(numeric_out,
              PointerGetDatum(v->val.numeric))));
      break;
    case jbvBinary:
      {
        StringInfo  jtext = makeStringInfo();
        (void) JsonbToCString(jtext, v->val.binary.data, -1);
        return cstring_to_text_with_len(jtext->data, jtext->len);
      }
      break;
    default:
      elog(ERROR, "Wrong jsonb type: %d", v->type);
  }
  return NULL;
}


JsonbValue toJsonbString(Datum str){
  JsonbValue	key;
  key.type = jbvString;
  key.val.string.val = VARDATA_ANY(str);
  key.val.string.len = VARSIZE_ANY_EXHDR(str);
  return key;
}

void walk(JbvResult *result, JsonbValue *jb, Datum *path, int level, int size){
  JsonbValue	key;
  JsonbValue *next_value;
  JsonbIterator *array_it;
  JsonbValue	array_value;
  int next_it;


  /* elog(DEBUG1,"%i %i", level, size); */
  /* debugJsonb(jb); */

  if (level == size){
    addToJbvResult(result, jb);
    return;
  }else{
    /* elog(DEBUG1, "take %s of %s", TextDatumGetCString(path[level]), JsbvToStr(jb)); */
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

void debugJsonb(JsonbValue *jb){
  switch (jb->type)
  {
    case jbvNull:
      elog(INFO, "D:jbvNull");
      break;
    case jbvString:
      elog(INFO, "D:jbvString, %s", JsbvToStr(jb));
      break;
    case jbvNumeric:
      elog(INFO, "D:jbvNumeric, %s", JsbvToStr(jb));
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
      elog(DEBUG1, "D:jbvBinary");
      break;
    default:
      elog(DEBUG1, "D:other");
  }
}
