#ifndef RESPONSIVEINDEX_HTML_FRAGMENTS_H
#define RESPONSIVEINDEX_HTML_FRAGMENTS_H

#include <ngx_config.h>
#include <ngx_core.h>

#define DOCTYPE "<!DOCTYPE html>"
#define HTML_PRE_LANG "<html lang=\""
#define TAG_END "\">"
#define HTML_POST_LANG TAG_END
#define HEAD_START "<head>"
#define CHARSET "<meta charset=\"utf-8\">"
#define VIEWPORT "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
#define LINK_PRE_HREF "<link rel=\"stylesheet\" href=\""
#define LINK_POST_HREF TAG_END
#define TITLE_START "<title>"
#define TITLE_END "</title>"
#define STYLE_START "<style>"
#define STYLE_END "</style>"
#define HEAD_END "</head>"
#define BODY_START "<body>"
#define BODY_END "</body>"
#define DIV_START(class) "<div class=\"" class TAG_END
#define DIV_END "</div>"
#define CONTAINER "container-fluid"
#define ROW "row"
#define COL "col-md-12"
#define H1_START "<h1>"
#define H1_END "</h1>"
#define TABLE "table-responsive hidden-xs hidden-sm"
#define TABLE_START "<table class=\"table table-striped table-condensed\">"
#define THEAD_START "<thead>"
#define TR_START "<tr>"
#define TH_START "<th>"
#define TH_END "</th>"
#define TR_END "</tr>"
#define THEAD_END "</thead>"
#define TBODY_START "<tbody>"
#define TD_START "<td>"
#define TD_END "</td>"
#define TBODY_END "</tbody>"
#define TABLE_END "</table>"
#define UL_START "<ul class=\"list-group visible-xs visible-sm\">"
#define LI_START "<li class=\"list-group-item\">"
#define LI_END "</li>"
#define UL_END "</ul>"
#define HTML_END "</html>"
#define A_PRE_HREF "<a href=\""
#define A_END "</a>"


static ngx_str_t en = ngx_string("en");
static ngx_str_t bootstrapcdn = ngx_string("//maxcdn.bootstrapcdn.com/bootstrap/3.2.0/css/bootstrap.min.css");


/* Blocks of the HTML to be written in order, with runtime data in between. */


static ngx_str_t to_lang = ngx_string(
	DOCTYPE CRLF
	HTML_PRE_LANG
);


static ngx_str_t to_stylesheet = ngx_string(
	HTML_POST_LANG CRLF
	HEAD_START CRLF
	CHARSET CRLF
	VIEWPORT CRLF
	LINK_PRE_HREF
);


static ngx_str_t to_title = ngx_string(
	TAG_END CRLF
	STYLE_START CRLF
	"body {" CRLF
	"    word-wrap: break-word;" CRLF
	"}" CRLF
	"a {" CRLF
	"    display: block;" CRLF
	"    width: 100%;" CRLF
	"    height: 100%;" CRLF
	"}" CRLF
	STYLE_END CRLF
	TITLE_START
);


static ngx_str_t to_h1 = ngx_string(
	TITLE_END CRLF
	HEAD_END CRLF
	BODY_START CRLF
	DIV_START(CONTAINER) CRLF
	DIV_START(ROW) CRLF
	DIV_START(COL) CRLF
	H1_START
);


static ngx_str_t to_table_body = ngx_string(
	H1_END CRLF
	DIV_START(TABLE) CRLF
	TABLE_START CRLF
	THEAD_START CRLF
	TR_START
	TH_START "File Name" TH_END
	TH_START "Date" TH_END
	TH_START "File Size" TH_END
	TR_END CRLF
	THEAD_END CRLF
	TBODY_START CRLF
	TR_START
	TD_START A_PRE_HREF ".." TAG_END ".." A_END TD_END
	TD_START TD_END
	TD_START TD_END
	TR_END CRLF
);


static ngx_str_t to_td_href = ngx_string(
	TR_START
	TD_START
	A_PRE_HREF
);


static ngx_str_t to_td_date = ngx_string(
	A_END
	TD_END
	TD_START
);


static ngx_str_t to_td_size = ngx_string(
	TD_END
	TD_START
);


static ngx_str_t end_row = ngx_string(
	TD_END
	TR_END CRLF);


static ngx_str_t to_list = ngx_string(
	TBODY_END CRLF
	TABLE_END CRLF
	DIV_END CRLF
	UL_START CRLF
	LI_START
	A_PRE_HREF
	".."
	TAG_END
	".."
	A_END
	LI_END CRLF
);


static ngx_str_t to_item_href = ngx_string(
	LI_START
	A_PRE_HREF
);


static ngx_str_t to_item_end = ngx_string(
	A_END
	LI_END CRLF);


static ngx_str_t to_html_end = ngx_string(
	UL_END CRLF
	DIV_END CRLF
	DIV_END CRLF
	DIV_END CRLF
	BODY_END CRLF
	HTML_END CRLF
);


#endif

