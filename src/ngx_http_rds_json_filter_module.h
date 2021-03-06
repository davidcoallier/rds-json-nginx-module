
/*
 * Copyright (C) agentzh
 */

#ifndef NGX_HTTP_RDS_JSON_FILTER_MODULE_H
#define NGX_HTTP_RDS_JSON_FILTER_MODULE_H

#include "ngx_http_rds.h"

#include <ngx_core.h>
#include <ngx_http.h>
#include <nginx.h>


#ifndef NGX_HTTP_RESET_CONTENT
#define NGX_HTTP_RESET_CONTENT 205
#endif


extern ngx_module_t  ngx_http_rds_json_filter_module;

extern ngx_http_output_header_filter_pt  ngx_http_rds_json_next_header_filter;

extern ngx_http_output_body_filter_pt    ngx_http_rds_json_next_body_filter;


typedef enum {
    json_format_compact,
    json_format_pretty          /* TODO */

} ngx_http_rds_json_format_t;


typedef struct {
    ngx_flag_t                       enabled;
    ngx_uint_t                       format;
    ngx_str_t                        content_type;

    ngx_str_t                        errcode;
    ngx_http_complex_value_t        *errstr;

    size_t                           buf_size;
} ngx_http_rds_json_conf_t;


typedef enum {
    state_expect_header,
    state_expect_col,
    state_expect_row,
    state_expect_field,
    state_expect_more_field_data,
    state_done

} ngx_http_rds_json_state_t;


typedef struct {
    ngx_http_rds_json_state_t            state;

    ngx_str_t                           *col_name;
    ngx_uint_t                           col_count;
    ngx_uint_t                           cur_col;

    ngx_http_rds_column_t               *cols;
    size_t                               row;

    uint32_t                             field_offset;
    uint32_t                             field_total;

    ngx_buf_tag_t                        tag;

    ngx_chain_t                         *out;
    ngx_chain_t                        **last_out;
    ngx_chain_t                         *busy_bufs;
    ngx_chain_t                         *free_bufs;

    ngx_buf_t                           *out_buf;
    ngx_buf_t                            cached;
    ngx_buf_t                            postponed;

    size_t                               avail_out;

    uint32_t                             field_data_rest;

    ngx_flag_t                           header_sent;

    ngx_flag_t                           seen_stream_end;
} ngx_http_rds_json_ctx_t;


#endif /* NGX_HTTP_RDS_JSON_FILTER_MODULE_H */

