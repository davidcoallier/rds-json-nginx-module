
/*
 * Copyright (C) agentzh
 */

#ifndef NGX_HTTP_RDS_H
#define NGX_HTTP_RDS_H


#include <nginx.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    uint16_t        std_errcode;
    uint16_t        drv_errcode;
    ngx_str_t       errstr;

    uint64_t        affected_rows;
    uint64_t        insert_id;
    uint16_t        column_count;

} ngx_http_rds_header_t;


static ngx_int_t
ngx_http_rds_parse_header(ngx_http_request_t *r, ngx_buf_t *b,
        ngx_http_rds_header_t *header)
{
    size_t          rest;

    rest = sizeof(uint8_t)      /* endian type */
         + sizeof(uint32_t)     /* format version */
         + sizeof(uint8_t)      /* result type */

         + sizeof(uint16_t)     /* standard error code */
         + sizeof(uint16_t)     /* driver-specific error code */

         + sizeof(uint16_t)     /* driver-specific errstr len */
         + 0                    /* driver-specific errstr data */
         + sizeof(uint64_t)     /* affected rows */
         + sizeof(uint64_t)     /* insert id */
         + sizeof(uint16_t)     /* column count */
         ;

    if (b->last - b->pos < rest) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
               "rds: header is incomplete in the buf");
        return NGX_ERROR;
    }

    /* check endian type */

    if ( *(uint8_t *) b->pos !=
#if (NGX_HAVE_LITTLE_ENDIAN)
            0
#else /* big endian */
            1
#endif
       )
    {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
               "rds: endian type in the header differ");
        return NGX_ERROR;
    }

    b->pos += sizeof(uint8_t);

    /* check RDS format version number */

    if ( *(uint32_t *) b->pos != (uint32_t) resty_dbd_stream_version) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
               "rds: RDS format version differ");
        return NGX_ERROR;
    }

    b->pos += sizeof(uint32_t);

    /* check RDS format type */

    if (*b->pos != 0) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
               "rds: RDS format type must be 0 for now");
        return NGX_ERROR;
    }

    b->pos++;

    /* save the standard error code */

    header->std_errcode = *(uint16_t) b->pos;

    b->pos += sizeof(uint16_t);

    /* save the driver-specific error code */

    header->drv_errcode = *(uint16_t) b->pos;

    b->pos += sizeof(uint16_t);

    /* save the error string length */

    header->errstr.len = *(uint16_t) b->pos;

    b->pos += sizeof(uint16_t);

    /* check the rest data's size */

    rest = header->errstr.len
         + sizeof(uint64_t)     /* affected rows */
         + sizeof(uint64_t)     /* insert id */
         + sizeof(uint16_t)     /* column count */
         ;

    if (b->last - b->pos < rest) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
               "rds: header is incomplete in the buf");
        return NGX_ERROR;
    }

    /* save the error string data */

    header->errstr.data = b->pos;

    b->pos += header->errstr.len;

    /* save affected rows */

    header->affected_rows = *(uint64_t *) b->pos;

    b->pos += sizeof(uint64_t);

    header->insert_id = *(uint64_t *)b->pos;

    b->pos += sizeof(uint64_t);

    header->column_count = *(uint16_t *) b->pos;

    b->pos += sizeof(uint16_t);

    return NGX_OK;
}


#endif /* NGX_HTTP_RDS_H */
