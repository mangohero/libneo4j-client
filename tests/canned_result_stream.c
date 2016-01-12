/* vi:set ts=4 sw=4 expandtab:
 *
 * Copyright 2016, Chris Leishman (http://github.com/cleishm)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "../config.h"
#include "canned_result_stream.h"
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <stddef.h>


typedef struct canned_result canned_result_t;
struct canned_result
{
    neo4j_result_t _result;

    neo4j_value_t list;
};


typedef struct canned_result_stream canned_result_stream_t;
struct canned_result_stream
{
    neo4j_result_stream_t _result_stream;

    const char * const *fieldnames;
    unsigned int nfields;
    struct canned_result *results;
    size_t nresults;
    size_t next_result;

    const char *error_message;
};
static_assert(offsetof(struct canned_result_stream, _result_stream) == 0,
        "_result_stream must be first field in struct canned_result_stream");


static int crs_check_failure(neo4j_result_stream_t *self);
static const char *crs_error_code(neo4j_result_stream_t *self);
static const char *crs_error_message(neo4j_result_stream_t *self);
static unsigned int crs_nfields(neo4j_result_stream_t *self);
static const char *crs_fieldname(neo4j_result_stream_t *self,
        unsigned int index);
static neo4j_result_t *crs_fetch_next(neo4j_result_stream_t *self);
static int crs_close(neo4j_result_stream_t *self);
static neo4j_value_t cr_canned_field(const neo4j_result_t *result,
        unsigned int index);
static neo4j_result_t *cr_canned_retain(neo4j_result_t *result);
static void cr_canned_release(neo4j_result_t *result);


neo4j_result_stream_t *neo4j_canned_result_stream(
        const char * const *fieldnames, unsigned int nfields,
        const neo4j_value_t *records, size_t nrecords)
{
    canned_result_t *cr = (nrecords > 0)?
        calloc(nrecords, sizeof(canned_result_t)) : NULL;
    if (nrecords > 0 && cr == NULL)
    {
        return NULL;
    }

    for (size_t i = 0; i < nrecords; ++i)
    {
        neo4j_result_t *result = (neo4j_result_t *)&(cr[i]);
        result->field = cr_canned_field;
        result->retain = cr_canned_retain;
        result->release = cr_canned_release;
        cr[i].list = records[i];
    }

    canned_result_stream_t *crs = calloc(1, sizeof(canned_result_stream_t));
    crs->fieldnames = fieldnames;
    crs->nfields = nfields;
    crs->results = cr;
    crs->nresults = nrecords;

    neo4j_result_stream_t *rs = (neo4j_result_stream_t *)crs;
    rs->check_failure = crs_check_failure;
    rs->error_code = crs_error_code;
    rs->error_message = crs_error_message;
    rs->nfields = crs_nfields;
    rs->fieldname = crs_fieldname;
    rs->fetch_next = crs_fetch_next;
    rs->close = crs_close;
    return rs;
}


void neo4j_crs_set_error(neo4j_result_stream_t *results, const char *msg)
{
    canned_result_stream_t *crs = (canned_result_stream_t *)results;
    crs->error_message = msg;
}


int crs_check_failure(neo4j_result_stream_t *self)
{
    canned_result_stream_t *crs = (canned_result_stream_t *)self;
    return (crs->error_message == NULL)? 0 : 1;
}


const char *crs_error_code(neo4j_result_stream_t *self)
{
    return NULL;
}


const char *crs_error_message(neo4j_result_stream_t *self)
{
    canned_result_stream_t *crs = (canned_result_stream_t *)self;
    return crs->error_message;
}


unsigned int crs_nfields(neo4j_result_stream_t *self)
{
    canned_result_stream_t *crs = (canned_result_stream_t *)self;
    return crs->nfields;
}


const char *crs_fieldname(neo4j_result_stream_t *self,
        unsigned int index)
{
    canned_result_stream_t *crs = (canned_result_stream_t *)self;
    if (index >= crs->nfields)
    {
        return NULL;
    }
    return crs->fieldnames[index];
}


neo4j_result_t *crs_fetch_next(neo4j_result_stream_t *self)
{
    canned_result_stream_t *crs = (canned_result_stream_t *)self;
    if (crs->next_result >= crs->nresults)
    {
        return NULL;
    }
    return (neo4j_result_t *)&(crs->results[(crs->next_result)++]);
}


int crs_close(neo4j_result_stream_t *self)
{
    canned_result_stream_t *crs = (canned_result_stream_t *)self;
    free(crs->results);
    free(crs);
    return 0;
}


neo4j_value_t cr_canned_field(const neo4j_result_t *result, unsigned int index)
{
    const canned_result_t *canned_result = (const canned_result_t *)result;
    return neo4j_list_get(canned_result->list, index);
}


neo4j_result_t *cr_canned_retain(neo4j_result_t *result)
{
    errno = ENOTSUP;
    return NULL;
}


void cr_canned_release(neo4j_result_t *result)
{
}
