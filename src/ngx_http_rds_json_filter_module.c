
/*
 * Copyright (C) agentzh
 */


#include "ngx_http_rds_json_filter_module.h"
#include <ngx_config.h>


static ngx_conf_enum_t  ngx_http_rds_json_formats[] = {
    { ngx_string("none"),    json_format_none },
    { ngx_string("compact"), json_format_compat },
    { ngx_string("pretty"),  json_format_pretty },
    { ngx_null_string, 0 }
};


ngx_http_output_header_filter_pt  ngx_http_rds_json_next_header_filter;
ngx_http_output_body_filter_pt    ngx_http_rds_json_next_body_filter;


static void *ngx_http_rds_json_create_conf(ngx_conf_t *cf);
static char *ngx_http_rds_json_merge_conf(ngx_conf_t *cf, void *parent,
    void *child);
static ngx_int_t ngx_http_rds_json_filter_init(ngx_conf_t *cf);


static ngx_command_t  ngx_http_rds_json_commands[] = {

    { ngx_string("rds_json"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF
          |NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
          |NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_rds_json_conf_t, format),
      &ngx_http_rds_json_formats },

    { ngx_string("rds_json_content_type"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF
          |NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF
          |NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_rds_json_conf_t, content_type),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_rds_json_filter_module_ctx = {
    NULL,                                  /* preconfiguration */
    ngx_http_rds_json_filter_init,         /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    ngx_http_rds_json_create_conf,         /* create location configuration */
    ngx_http_rds_json_merge_conf           /* merge location configuration */
};


ngx_module_t  ngx_http_rds_json_filter_module = {
    NGX_MODULE_V1,
    &ngx_http_rds_json_filter_module_ctx,  /* module context */
    ngx_http_rds_json_commands,            /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_rds_json_header_filter(ngx_http_request_t *r)
{
    ngx_http_rds_json_ctx_t   *ctx;
    ngx_http_rds_json_conf_t  *conf;

    if (r->headers_out.status != NGX_HTTP_OK || r != r->main) {
        return ngx_http_next_header_filter(r);
    }

    conf = ngx_http_get_module_loc_conf(r, ngx_http_rds_json_filter_module);

    if (conf->before_body.len == 0 && conf->after_body.len == 0) {
        return ngx_http_next_header_filter(r);
    }

    if (ngx_http_test_content_type(r, &conf->types) == NULL) {
        return ngx_http_next_header_filter(r);
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_rds_json_ctx_t));
    if (ctx == NULL) {
        return NGX_ERROR;
    }

    ngx_http_set_ctx(r, ctx, ngx_http_rds_json_filter_module);

    ngx_http_clear_content_length(r);
    ngx_http_clear_accept_ranges(r);

    return ngx_http_next_header_filter(r);
}


static ngx_int_t
ngx_http_rds_json_body_filter(ngx_http_request_t *r, ngx_chain_t *in)
{
    ngx_int_t                  rc;
    ngx_uint_t                 last;
    ngx_chain_t               *cl;
    ngx_http_request_t        *sr;
    ngx_http_rds_json_ctx_t   *ctx;
    ngx_http_rds_json_conf_t  *conf;

    if (in == NULL || r->header_only) {
        return ngx_http_next_body_filter(r, in);
    }

    ctx = ngx_http_get_module_ctx(r, ngx_http_rds_json_filter_module);

    if (ctx == NULL) {
        return ngx_http_next_body_filter(r, in);
    }

    conf = ngx_http_get_module_loc_conf(r, ngx_http_rds_json_filter_module);

    if (!ctx->before_body_sent) {
        ctx->before_body_sent = 1;

        if (conf->before_body.len) {
            if (ngx_http_subrequest(r, &conf->before_body, NULL, &sr, NULL, 0)
                != NGX_OK)
            {
                return NGX_ERROR;
            }
        }
    }

    if (conf->after_body.len == 0) {
        ngx_http_set_ctx(r, NULL, ngx_http_rds_json_filter_module);
        return ngx_http_next_body_filter(r, in);
    }

    last = 0;

    for (cl = in; cl; cl = cl->next) {
        if (cl->buf->last_buf) {
            cl->buf->last_buf = 0;
            cl->buf->sync = 1;
            last = 1;
        }
    }

    rc = ngx_http_next_body_filter(r, in);

    if (rc == NGX_ERROR || !last || conf->after_body.len == 0) {
        return rc;
    }

    if (ngx_http_subrequest(r, &conf->after_body, NULL, &sr, NULL, 0)
        != NGX_OK)
    {
        return NGX_ERROR;
    }

    ngx_http_set_ctx(r, NULL, ngx_http_rds_json_filter_module);

    return ngx_http_send_special(r, NGX_HTTP_LAST);
}


static ngx_int_t
ngx_http_rds_json_filter_init(ngx_conf_t *cf)
{
    ngx_http_next_header_filter = ngx_http_top_header_filter;
    ngx_http_top_header_filter = ngx_http_rds_json_header_filter;

    ngx_http_next_body_filter = ngx_http_top_body_filter;
    ngx_http_top_body_filter = ngx_http_rds_json_body_filter;

    return NGX_OK;
}


static void *
ngx_http_rds_json_create_conf(ngx_conf_t *cf)
{
    ngx_http_rds_json_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_rds_json_conf_t));
    if (conf == NULL) {
        return NULL;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     conf->before_body = { 0, NULL };
     *     conf->after_body = { 0, NULL };
     *     conf->types = { NULL };
     *     conf->types_keys = NULL;
     */

    return conf;
}


static char *
ngx_http_rds_json_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_rds_json_conf_t *prev = parent;
    ngx_http_rds_json_conf_t *conf = child;

    ngx_conf_merge_str_value(conf->before_body, prev->before_body, "");
    ngx_conf_merge_str_value(conf->after_body, prev->after_body, "");

    if (ngx_http_merge_types(cf, &conf->types_keys, &conf->types,
                             &prev->types_keys, &prev->types,
                             ngx_http_html_default_types)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}