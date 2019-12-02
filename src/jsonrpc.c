/*
 * Copyright (c) 2020 Elastos Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <string.h>
#include <stdlib.h>

#include "jsonrpc.h"

#define is_structured(json) (cJSON_IsObject((json)) || cJSON_IsArray((json)))

static
JsonRPCType jsonrpc_parse_obj(const cJSON *obj)
{
    cJSON *jsonrpc;
    cJSON *method;
    cJSON *result;
    cJSON *error;
    cJSON *id;

    jsonrpc = cJSON_GetObjectItemCaseSensitive(obj, "jsonrpc");
    if (!jsonrpc || !cJSON_IsString(jsonrpc) || strcmp(jsonrpc->valuestring, "2.0"))
        return JSONRPC_TYPE_INVALID;

    method = cJSON_GetObjectItemCaseSensitive(obj, "method");
    result = cJSON_GetObjectItemCaseSensitive(obj, "result");
    error = cJSON_GetObjectItemCaseSensitive(obj, "error");
    if (!!method + !!result + !!error != 1)
        return JSONRPC_TYPE_INVALID;

    id = cJSON_GetObjectItemCaseSensitive(obj, "id");
    if (id && !cJSON_IsString(id) && !cJSON_IsNumber(id) && !cJSON_IsNull(id))
        return JSONRPC_TYPE_INVALID;

    if (method) {
        cJSON *params;

        if (!cJSON_IsString(method) || !method->valuestring[0])
            return JSONRPC_TYPE_INVALID;

        params = cJSON_GetObjectItemCaseSensitive(obj, "params");
        if (params && !is_structured(params))
            return JSONRPC_TYPE_INVALID;

        return id ? JSONRPC_TYPE_REQUEST : JSONRPC_TYPE_NOTIFICATION;
    } else {
        if (!id)
            return JSONRPC_TYPE_INVALID;

        if (result)
            return JSONRPC_TYPE_SUCCESS_RESPONSE;
        else {
            cJSON *code;
            cJSON *message;

            code = cJSON_GetObjectItemCaseSensitive(error, "code");
            if (!code || !cJSON_IsNumber(code))
                return JSONRPC_TYPE_INVALID;

            message = cJSON_GetObjectItemCaseSensitive(error, "message");
            if (!message || !cJSON_IsString(message) || !message->valuestring[0])
                return JSONRPC_TYPE_INVALID;

            return JSONRPC_TYPE_ERROR_RESPONSE;
        }
    }
}

JsonRPCType jsonrpc_decode(const void *json, cJSON **prpc, JsonRPCBatchInfo **batch_info)
{
    cJSON *rpc;
    JsonRPCBatchInfo *bi;
    int nobjs;
    int i;

    rpc = cJSON_Parse(json);
    if (!rpc) {
        *prpc = NULL;
        *batch_info = NULL;
        return JSONRPC_TYPE_INVALID;
    }

    if (!cJSON_IsArray(rpc)) {
        *prpc = rpc;
        *batch_info = NULL;
        return jsonrpc_parse_obj(rpc);
    }

    nobjs = cJSON_GetArraySize(rpc);
    if (!nobjs) {
        *prpc = rpc;
        *batch_info = NULL;
        return JSONRPC_TYPE_INVALID;
    }

    bi = malloc(sizeof(JsonRPCBatchInfo) + sizeof(JsonRPCType) * nobjs);
    if (!bi) {
        *prpc = rpc;
        *batch_info = NULL;
        return JSONRPC_TYPE_INVALID;
    }
    bi->nobjs = nobjs;

    for (i = 0; i < nobjs; ++i)
        bi->obj_types[i] = jsonrpc_parse_obj(cJSON_GetArrayItem(rpc, i));

    *prpc = rpc;
    *batch_info = bi;
    return JSONRPC_TYPE_BATCH;
}

cJSON *jsonrpc_encode_request(const char *method, cJSON *params, cJSON *id)
{
    cJSON *rpc;

    rpc = cJSON_CreateObject();
    if (!rpc)
        goto failure;

    if (!cJSON_AddStringToObject(rpc, "jsonrpc", "2.0"))
        goto failure;

    if (!cJSON_AddStringToObject(rpc, "method", method))
        goto failure;

    if (params) {
        cJSON_AddItemToObject(rpc, "params", params);
        params = NULL;
    }

    if (!id) {
        id = cJSON_CreateNull();
        if (!id)
            goto failure;
    }

    cJSON_AddItemToObject(rpc, "id", id);

    return rpc;

failure:
    if (rpc)
        cJSON_Delete(rpc);

    if (params)
        cJSON_Delete(params);

    if (id)
        cJSON_Delete(id);

    return NULL;
}

cJSON *jsonrpc_encode_notification(const char *method, cJSON *params)
{
    cJSON *rpc;

    rpc = cJSON_CreateObject();
    if (!rpc)
        goto failure;

    if (!cJSON_AddStringToObject(rpc, "jsonrpc", "2.0"))
        goto failure;

    if (!cJSON_AddStringToObject(rpc, "method", method))
        goto failure;

    if (params)
        cJSON_AddItemToObject(rpc, "params", params);

    return rpc;

failure:
    if (rpc)
        cJSON_Delete(rpc);

    if (params)
        cJSON_Delete(params);

    return NULL;
}

cJSON *jsonrpc_encode_success_response(cJSON *result, cJSON *id)
{
    cJSON *rpc;

    rpc = cJSON_CreateObject();
    if (!rpc)
        goto failure;

    if (!cJSON_AddStringToObject(rpc, "jsonrpc", "2.0"))
        goto failure;

    if (!result) {
        result = cJSON_CreateNull();
        if (!result)
            goto failure;
    }
    cJSON_AddItemToObject(rpc, "result", result);
    result = NULL;

    if (!id) {
        id = cJSON_CreateNull();
        if (!id)
            goto failure;
    }
    cJSON_AddItemToObject(rpc, "id", id);

    return rpc;

failure:
    if (rpc)
        cJSON_Delete(rpc);

    if (result)
        cJSON_Delete(result);

    if (id)
        cJSON_Delete(id);

    return NULL;
}

cJSON *jsonrpc_encode_error_response(int code, const char *msg, cJSON *data, cJSON *id)
{
    cJSON *rpc;
    cJSON *error;
    char *encoded;

    rpc = cJSON_CreateObject();
    if (!rpc)
        goto failure;

    if (!cJSON_AddStringToObject(rpc, "jsonrpc", "2.0"))
        goto failure;

    error = cJSON_AddObjectToObject(rpc, "error");
    if (!error)
        goto failure;

    if (!cJSON_AddNumberToObject(error, "code", code))
        goto failure;

    if (!cJSON_AddStringToObject(error, "message", msg))
        goto failure;

    if (data) {
        cJSON_AddItemToObject(error, "data", data);
        data = NULL;
    }

    if (!id) {
        id = cJSON_CreateNull();
        if (!id)
            goto failure;
    }

    cJSON_AddItemToObject(rpc, "id", id);

    return rpc;

failure:
    if (rpc)
        cJSON_Delete(rpc);

    if (data)
        cJSON_Delete(data);

    if (id)
        cJSON_Delete(id);

    return NULL;
}

static struct {
    int code;
    const char *message;
} err_msgs[] = {
    {JSONRPC_EINVALID_REQUEST , "Invalid Request"   },
    {JSONRPC_EMETHOD_NOT_FOUND, "Method not found"  },
    {JSONRPC_EINVALID_PARAMS  , "Invalid Parameters"},
    {JSONRPC_EINTERNAL_ERROR  , "Internal Error"    },
    {JSONRPC_EPARSE_ERROR     , "Parse Error"       }
};

const char *jsonrpc_error_message(int err_code)
{
    int i;

    for (i = 0; i < sizeof(err_msgs) / sizeof(err_msgs[0]); ++i) {
        if (err_code == err_msgs[i].code)
            return err_msgs[i].message;
    }

    return NULL;
}

const char *jsonrpc_get_method(const cJSON *json)
{
    return cJSON_GetObjectItemCaseSensitive(json, "method")->valuestring;
}

cJSON *jsonrpc_get_params(const cJSON *json)
{
    return cJSON_GetObjectItemCaseSensitive(json, "params");
}

cJSON *jsonrpc_get_result(const cJSON *json)
{
    return cJSON_GetObjectItemCaseSensitive(json, "result");
}

cJSON *jsonrpc_get_id(const cJSON *json)
{
    return cJSON_GetObjectItemCaseSensitive(json, "id");
}

cJSON *jsonrpc_batch_get_object(const cJSON *json, int idx)
{
    return cJSON_GetArrayItem(json, idx);
}

cJSON *jsonrpc_encode_empty_batch()
{
    return cJSON_CreateArray();
}

void jsonrpc_add_object_to_batch(cJSON *batch, cJSON *obj)
{
    cJSON_AddItemToArray(batch, obj);
}

int jsonrpc_get_error_code(const cJSON *json)
{
    return (int)cJSON_GetObjectItemCaseSensitive(
                cJSON_GetObjectItemCaseSensitive(json, "error"),
                "code")->valuedouble;
}

const char *jsonrpc_get_error_message(const cJSON *json)
{
    return cJSON_GetObjectItemCaseSensitive(
               cJSON_GetObjectItemCaseSensitive(json, "error"),
               "message")->valuestring;
}
