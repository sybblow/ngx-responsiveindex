
/*
 * Uses code from ngx_http_autoindex_e_module.c
 * Written by Casper Jørgensen
 *
 * Modified from ngx_http_autoindex_module.c
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */



#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


#include "html_fragments.h"

typedef struct {
	ngx_str_t	name;
	size_t		utf_len;
	size_t		escape;
	size_t		escape_html;

	unsigned	is_dir:1;

	time_t		mtime;
	off_t		size;
} ngx_http_responsiveindex_entry_t;


typedef struct {
	ngx_flag_t	enable;
	ngx_flag_t	localtime;
	ngx_flag_t	exact_size;

	/* URI to load the Twitter bootstrap CSS from. */
	ngx_str_t	bootstrap_href;

	/* html LANG attribute value. */
	ngx_str_t	lang;

} ngx_http_responsiveindex_loc_conf_t;


#define NGX_HTTP_AUTOINDEX_PREALLOCATE	255
static int ngx_libc_cdecl ngx_http_responsiveindex_cmp_entries(const void *one,
		const void *two);
static ngx_int_t ngx_http_responsiveindex_error(ngx_http_request_t *r,
		ngx_dir_t *dir, ngx_str_t *name);
static ngx_int_t ngx_http_responsiveindex_init(ngx_conf_t *cf);
static void *ngx_http_responsiveindex_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_responsiveindex_merge_loc_conf(ngx_conf_t *cf,
		void *parent, void *child);

static void ngx_http_responsiveindex_cpy_uri(ngx_buf_t *, ngx_http_responsiveindex_entry_t *);

static void ngx_http_responsiveindex_cpy_size(ngx_buf_t *, ngx_http_responsiveindex_entry_t *,
		ngx_http_responsiveindex_loc_conf_t  *);


static ngx_command_t  ngx_http_responsiveindex_commands[] = {

	{
		ngx_string("responsiveindex"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
		ngx_conf_set_flag_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_responsiveindex_loc_conf_t, enable),
		NULL
	},

	{
		ngx_string("responsiveindex_localtime"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
		ngx_conf_set_flag_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_responsiveindex_loc_conf_t, localtime),
		NULL
	},

	{
		ngx_string("responsiveindex_exact_size"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
		ngx_conf_set_flag_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_responsiveindex_loc_conf_t, exact_size),
		NULL
	},

	{
		ngx_string("responsiveindex_bootstrap_href"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_responsiveindex_loc_conf_t, bootstrap_href),
		NULL
	},

	{
		ngx_string("responsiveindex_lang"),
		NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_FLAG,
		ngx_conf_set_str_slot,
		NGX_HTTP_LOC_CONF_OFFSET,
		offsetof(ngx_http_responsiveindex_loc_conf_t, lang),
		NULL
	},


	ngx_null_command
};


static ngx_http_module_t ngx_http_responsiveindex_module_ctx = {

	/* preconfiguration */
	NULL,

	/* postconfiguration */
	ngx_http_responsiveindex_init,

	/* create main configuration */
	NULL,

	/* init main configuration */
	NULL,

	/* create server configuration */
	NULL,

	/* merge server configuration */
	NULL,

	/* create location configuration */
	ngx_http_responsiveindex_create_loc_conf,

	/* merge location configuration */
	ngx_http_responsiveindex_merge_loc_conf
};


ngx_module_t ngx_http_responsiveindex_module = {
	NGX_MODULE_V1,

	/* module context */
	&ngx_http_responsiveindex_module_ctx,

	/* module directives */
	ngx_http_responsiveindex_commands,

	/* module type */
	NGX_HTTP_MODULE,

	/* init master */
	NULL,
	/* init module */
	NULL,

	/* init process */
	NULL,

	/* init thread */
	NULL,

	/* exit thread */
	NULL,

	/* exit process */
	NULL,

	/* exit master */
	NULL,

	NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_responsiveindex_handler(ngx_http_request_t *r)
{
	u_char						*last, *filename;
	size_t						length, escape_html, allocated, root, response_size;
	ngx_tm_t					tm;
	ngx_err_t					err;
	ngx_buf_t					*b;
	ngx_int_t					rc;
	ngx_str_t					path;
	ngx_dir_t					dir;
	ngx_uint_t					i, level, utf8;
	ngx_pool_t					*pool;
	ngx_time_t					*tp;
	ngx_chain_t					out;
	ngx_array_t					entries;
	ngx_http_responsiveindex_entry_t	 *entry;
	ngx_http_responsiveindex_loc_conf_t *conf;

	static char *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

	/* Only handle folders (this will allow files to be served). */
	if (r->uri.data[r->uri.len - 1] != '/') {
		return NGX_DECLINED;
	}

	/* This module ony handles GET and HEAD requests. */
	if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {
		return NGX_DECLINED;
	}

	/* First we need to have access to the config. */
	conf = ngx_http_get_module_loc_conf(r, ngx_http_responsiveindex_module);

	/* Make sure indexing is enabled. */
	if (!conf->enable) {
		return NGX_DECLINED;
	}

	/* NGX_DIR_MASK_LEN is lesser than NGX_HTTP_AUTOINDEX_PREALLOCATE */

	/* Map the URI to a path. */
	last = ngx_http_map_uri_to_path(r, &path, &root,
			NGX_HTTP_AUTOINDEX_PREALLOCATE);
	if (last == NULL) {
		return NGX_HTTP_INTERNAL_SERVER_ERROR;
	}

	/* Set the actual path size. */
	allocated = path.len;
	path.len = last - path.data;
	if (path.len > 1) {
		path.len--;
	}
	path.data[path.len] = '\0';

	ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
			"http responsiveindex: \"%s\"", path.data);

	/* Open the path for reading. */
	if (ngx_open_dir(&path, &dir) == NGX_ERROR) {
		err = ngx_errno;

		if (err == NGX_ENOENT
				|| err == NGX_ENOTDIR
				|| err == NGX_ENAMETOOLONG)
		{
			level = NGX_LOG_ERR;
			rc = NGX_HTTP_NOT_FOUND;

		} else if (err == NGX_EACCES) {
			level = NGX_LOG_ERR;
			rc = NGX_HTTP_FORBIDDEN;

		} else {
			level = NGX_LOG_CRIT;
			rc = NGX_HTTP_INTERNAL_SERVER_ERROR;
		}

		ngx_log_error(level, r->connection->log, err,
				ngx_open_dir_n " \"%s\" failed", path.data);

		return rc;
	}

#if (NGX_SUPPRESS_WARN)

	/* MSVC thinks 'entries' may be used without having been initialized */
	ngx_memzero(&entries, sizeof(ngx_array_t));

#endif

	/* TODO: pool should be temporary pool */
	pool = r->pool;

	if (ngx_array_init(&entries, pool, 40, sizeof(ngx_http_responsiveindex_entry_t))
			!= NGX_OK)
	{
		return ngx_http_responsiveindex_error(r, &dir, &path);
	}

	/* Set the headers (the response is HTML). */
	r->headers_out.status = NGX_HTTP_OK;
	r->headers_out.content_type_len = sizeof("text/html") - 1;
	ngx_str_set(&r->headers_out.content_type, "text/html");
	r->headers_out.content_type_lowcase = NULL;

	rc = ngx_http_send_header(r);

	if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
		if (ngx_close_dir(&dir) == NGX_ERROR) {
			ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
					ngx_close_dir_n " \"%V\" failed", &path);
		}

		return rc;
	}

	filename = path.data;
	filename[path.len] = '/';

	if (r->headers_out.charset.len == 5
			&& ngx_strncasecmp(r->headers_out.charset.data, (u_char *) "utf-8", 5)
			== 0)
	{
		utf8 = 1;

	} else {
		utf8 = 0;
	}

	/* Loop through all files. */
	for ( ;; ) {
		ngx_set_errno(0);

		/* Read the directory, break when there are no more files. */
		if (ngx_read_dir(&dir) == NGX_ERROR) {
			err = ngx_errno;

			if (err != NGX_ENOMOREFILES) {
				ngx_log_error(NGX_LOG_CRIT, r->connection->log, err,
						ngx_read_dir_n " \"%V\" failed", &path);
				return ngx_http_responsiveindex_error(r, &dir, &path);
			}

			break;
		}

		ngx_log_debug1(NGX_LOG_DEBUG_HTTP, r->connection->log, 0,
				"http responsiveindex file: \"%s\"", ngx_de_name(&dir));

		length = ngx_de_namelen(&dir);

		/* Skip hidden files and folders. */
		if (ngx_de_name(&dir)[0] == '.') {
			continue;
		}

		/* Get additional file info. */
		if (!dir.valid_info) {

			/* 1 byte for '/' and 1 byte for terminating '\0' */

			if (path.len + 1 + length + 1 > allocated) {
				allocated = path.len + 1 + length + 1
					+NGX_HTTP_AUTOINDEX_PREALLOCATE;

				filename = ngx_pnalloc(pool, allocated);
				if (filename == NULL) {
					return ngx_http_responsiveindex_error(r, &dir, &path);
				}

				/* Add the path to the filename and trailing slash. */
				last = ngx_cpystrn(filename, path.data, path.len + 1);
				*last++ = '/';
			}

			/* Copy the actual filename into the path. */
			ngx_cpystrn(last, ngx_de_name(&dir), length + 1);

			/* Get additional information about the file. */
			if (ngx_de_info(filename, &dir) == NGX_FILE_ERROR) {
				err = ngx_errno;

				if (err != NGX_ENOENT && err != NGX_ELOOP) {
					ngx_log_error(NGX_LOG_CRIT, r->connection->log, err,
							ngx_de_info_n " \"%s\" failed", filename);

					if (err == NGX_EACCES) {
						continue;
					}

					return ngx_http_responsiveindex_error(r, &dir, &path);
				}

				if (ngx_de_link_info(filename, &dir) == NGX_FILE_ERROR) {
					ngx_log_error(NGX_LOG_CRIT, r->connection->log, ngx_errno,
							ngx_de_link_info_n " \"%s\" failed",
							filename);
					return ngx_http_responsiveindex_error(r, &dir, &path);
				}
			}
		}

		/* Push an entry into the array. */
		entry = ngx_array_push(&entries);
		if (entry == NULL) {
			return ngx_http_responsiveindex_error(r, &dir, &path);
		}

		/* Allocate memory for the file name. */
		entry->name.len = length;
		entry->name.data = ngx_pnalloc(pool, length + 1);

		/* Make sure we have allocated memory. */
		if (entry->name.data == NULL) {
			return ngx_http_responsiveindex_error(r, &dir, &path);
		}

		/* Assign file name. */
		ngx_cpystrn(entry->name.data, ngx_de_name(&dir), length + 1);

		entry->escape = 2 * ngx_escape_uri(NULL, ngx_de_name(&dir), length,
				NGX_ESCAPE_URI_COMPONENT);

		entry->escape_html = ngx_escape_html(NULL, entry->name.data,
				entry->name.len);

		if (utf8) {
			entry->utf_len = ngx_utf8_length(entry->name.data, entry->name.len);
		} else {
			entry->utf_len = length;
		}

		/* Assign file attributes. */
		entry->is_dir = ngx_de_is_dir(&dir);
		entry->mtime = ngx_de_mtime(&dir);
		entry->size = ngx_de_size(&dir);
	}

	/* Close the directory. */
	if (ngx_close_dir(&dir) == NGX_ERROR) {
		ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
				ngx_close_dir_n " \"%V\" failed", &path);
	}

	escape_html = ngx_escape_html(NULL, r->uri.data, r->uri.len);

	/* We need to calculate the size of the buffer. */

	response_size = r->uri.len + escape_html
		+ r->uri.len + escape_html
		+ to_lang.len
		+ to_stylesheet.len
		+ to_title.len
		+ to_h1.len
		+ to_table_body.len
		;

	if (conf->lang.len) {
		response_size += conf->lang.len;
	} else {
		response_size += en.len;
	}


	if (conf->bootstrap_href.len) {
		response_size += conf->bootstrap_href.len;
	} else {
		response_size += bootstrapcdn.len;
	}

	/* Add a table with each file. */
	entry = entries.elts;
	for (i = 0; i < entries.nelts; i++) {
		response_size += entry[i].name.len + entry[i].escape

			/* 1 is for "/" */
			+ 1

			+ to_td_href.len
			+ tag_end.len
			+ entry[i].name.len - entry[i].utf_len
			+ entry[i].escape_html
			+ entry[i].name.len
			+ to_td_date.len
			+ sizeof("28-Sep-1970 12:00") - 1
			+ to_td_size.len

			/* the file size */
			+ 20

			+ end_row.len
			;
	}

	response_size += to_list.len;

	for (i = 0; i < entries.nelts; i++) {
		response_size += entry[i].name.len + entry[i].escape
			+ to_item_href.len
			+ tag_end.len
			+ entry[i].name.len - entry[i].utf_len
			+ entry[i].escape_html
			+ entry[i].name.len

			/* 1 is for "/" */
			+ 1

			+ sizeof("\">") - 1
			+ to_item_end.len
			;
	}


	response_size += to_html_end.len;

	/* Allocate a buffer for the response body based on the size we calculated. */
	b = ngx_create_temp_buf(r->pool, response_size);
	if (b == NULL) {
		return NGX_ERROR;
	}

	/* Sort the entries. */
	if (entries.nelts > 1) {
		ngx_qsort(entry, (size_t) entries.nelts,
				sizeof(ngx_http_responsiveindex_entry_t),
				ngx_http_responsiveindex_cmp_entries);
	}

	/* Start adding data to the response. */

	b->last = ngx_cpymem(b->last, to_lang.data, to_lang.len);

	if (conf->lang.len) {
		b->last = ngx_cpymem(b->last, conf->lang.data, conf->lang.len);
	} else {
		b->last = ngx_cpymem(b->last, en.data, en.len);
	}

	b->last = ngx_cpymem(b->last, to_stylesheet.data, to_stylesheet.len);

	if (conf->bootstrap_href.len) {
		b->last = ngx_cpymem(b->last, conf->bootstrap_href.data, conf->bootstrap_href.len);
	} else {
		b->last = ngx_cpymem(b->last, bootstrapcdn.data, bootstrapcdn.len);
	}

	b->last = ngx_cpymem(b->last, to_title.data, to_title.len);

	if (escape_html) {
		b->last = (u_char *) ngx_escape_html(b->last, r->uri.data, r->uri.len);
		b->last = ngx_cpymem(b->last, to_h1.data, to_h1.len);
		b->last = (u_char *) ngx_escape_html(b->last, r->uri.data, r->uri.len);
	} else {
		b->last = ngx_cpymem(b->last, r->uri.data, r->uri.len);
		b->last = ngx_cpymem(b->last, to_h1.data, to_h1.len);
		b->last = ngx_cpymem(b->last, r->uri.data, r->uri.len);
	}

	b->last = ngx_cpymem(b->last, to_table_body.data, to_table_body.len);

	tp = ngx_timeofday();

	for (i = 0; i < entries.nelts; i++) {

		b->last = ngx_cpymem(b->last, to_td_href.data, to_td_href.len);

		ngx_http_responsiveindex_cpy_uri(b, &entry[i]);

		b->last = ngx_cpymem(b->last, tag_end.data, tag_end.len);

		b->last = ngx_cpymem(b->last, entry[i].name.data, entry[i].name.len);

		b->last = ngx_cpymem(b->last, to_td_date.data, to_td_date.len);

		ngx_gmtime(entry[i].mtime + tp->gmtoff * 60 * conf->localtime, &tm);


		b->last = ngx_sprintf(b->last, "%02d-%s-%d %02d:%02d ",
				tm.ngx_tm_mday,
				months[tm.ngx_tm_mon - 1],
				tm.ngx_tm_year,
				tm.ngx_tm_hour,
				tm.ngx_tm_min);

		b->last = ngx_cpymem(b->last, to_td_size.data, to_td_size.len);

		ngx_http_responsiveindex_cpy_size(b, &entry[i], conf);

		b->last = ngx_cpymem(b->last, end_row.data, end_row.len);
	}

	b->last = ngx_cpymem(b->last, to_list.data, to_list.len);

	for (i = 0; i < entries.nelts; i++) {
		b->last = ngx_cpymem(b->last, to_item_href.data,to_item_href.len);

		ngx_http_responsiveindex_cpy_uri(b, &entry[i]);

		b->last = ngx_cpymem(b->last, tag_end.data, tag_end.len);

		b->last = ngx_cpymem(b->last, entry[i].name.data, entry[i].name.len);

		b->last = ngx_cpymem(b->last, to_item_end.data, to_item_end.len);

	}

	b->last = ngx_cpymem(b->last, to_html_end.data, to_html_end.len);

	/* TODO: free temporary pool */

	/* Last buffer in chain. */
	if (r == r->main) {
		b->last_buf = 1;
	}

	b->last_in_chain = 1;

	/* Attach this buffer to the buffer chain. */
	out.buf = b;
	out.next = NULL;

	/* Send the buffer chain of the response. */
	return ngx_http_output_filter(r, &out);
}


static int ngx_libc_cdecl
ngx_http_responsiveindex_cmp_entries(const void *one, const void *two)
{
	ngx_http_responsiveindex_entry_t *first = (ngx_http_responsiveindex_entry_t *) one;
	ngx_http_responsiveindex_entry_t *second = (ngx_http_responsiveindex_entry_t *) two;

	if (first->is_dir && !second->is_dir) {
		/* move the directories to the start */
		return -1;
	}

	if (!first->is_dir && second->is_dir) {
		/* move the directories to the start */
		return 1;
	}

	return (int) ngx_strcmp(first->name.data, second->name.data);
}


static ngx_int_t
ngx_http_responsiveindex_error(ngx_http_request_t *r, ngx_dir_t *dir, ngx_str_t *name)
{
	if (ngx_close_dir(dir) == NGX_ERROR) {
		ngx_log_error(NGX_LOG_ALERT, r->connection->log, ngx_errno,
				ngx_close_dir_n " \"%V\" failed", name);
	}

	return r->header_sent ? NGX_ERROR : NGX_HTTP_INTERNAL_SERVER_ERROR;
}


static void *
ngx_http_responsiveindex_create_loc_conf(ngx_conf_t *cf)
{
	ngx_http_responsiveindex_loc_conf_t  *conf;

	conf = ngx_palloc(cf->pool, sizeof(ngx_http_responsiveindex_loc_conf_t));
	if (conf == NULL) {
		return NULL;
	}

	conf->enable = NGX_CONF_UNSET;
	conf->localtime = NGX_CONF_UNSET;
	conf->exact_size = NGX_CONF_UNSET;

	return conf;
}


static char *
ngx_http_responsiveindex_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
	ngx_http_responsiveindex_loc_conf_t *prev = parent;
	ngx_http_responsiveindex_loc_conf_t *conf = child;

	ngx_conf_merge_value(conf->enable, prev->enable, 0);
	ngx_conf_merge_value(conf->localtime, prev->localtime, 0);
	ngx_conf_merge_value(conf->exact_size, prev->exact_size, 1);

	return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_responsiveindex_init(ngx_conf_t *cf)
{
	ngx_http_handler_pt		*h;
	ngx_http_core_main_conf_t  *cmcf;

	cmcf = ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);

	h = ngx_array_push(&cmcf->phases[NGX_HTTP_CONTENT_PHASE].handlers);
	if (h == NULL) {
		return NGX_ERROR;
	}

	*h = ngx_http_responsiveindex_handler;

	return NGX_OK;
}


static void
ngx_http_responsiveindex_cpy_uri(ngx_buf_t *b, ngx_http_responsiveindex_entry_t *entry)
{
	if (entry->escape) {
		ngx_escape_uri(b->last, entry->name.data, entry->name.len, NGX_ESCAPE_URI_COMPONENT);
		b->last += entry->name.len + entry->escape;
	} else {
		b->last = ngx_cpymem(b->last, entry->name.data, entry->name.len);
	}

	if (entry->is_dir) {
		*b->last++ = '/';
	}
}


static void
ngx_http_responsiveindex_cpy_size(ngx_buf_t *b, ngx_http_responsiveindex_entry_t *entry,
		ngx_http_responsiveindex_loc_conf_t  *conf) {

	/* Implementation taken from ngx-autoindex-ext */

	off_t s;
	size_t size;
	u_char scale;

	if (entry->is_dir)
		b->last = ngx_cpymem(b->last, "-", sizeof("-") - 1);
	else
	{
		if (conf->exact_size)
			b->last = ngx_sprintf(b->last, "%i", entry->size);
		else
		{
			s = entry->size;
			if (s > 1024 * 1024 - 1) {
				size = (ngx_int_t) (s / (1024 * 1024));
				if ((s % (1024 * 1024)) > (1024 * 1024 / 2 - 1)) {
					size++;
				}
				scale = 'M';
			} else if (s > 9999) {
				size = (ngx_int_t) (s / 1024);
				if (s % 1024 > 511) {
					size++;
				}
				scale = 'K';
			} else {
				size = (ngx_int_t)s;
				scale = '\0';
			}
			if (scale) {
				b->last = ngx_sprintf(b->last, "%i%c", size, scale);
			} else {
				b->last = ngx_sprintf(b->last, "%i", size);
			}
		}
	}
}
