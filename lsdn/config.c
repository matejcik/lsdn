#include "config.h"
#include "common.h"
#include "strbuf.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <yaml.h>


struct config_file {
	FILE *file;			/* config file handle */
	yaml_parser_t parser;		/* YAML parser */
	yaml_document_t doc;		/* YAML document */
	yaml_node_t *root_node;		/* YAML root node */
	bool error;			/* error flag */
	struct strbuf error_str_buf;	/* error string buffer */
};

struct config_file *config_file;


static bool load_config_item_value(struct config_item *item, yaml_node_t *node)
{
	switch (node->type) {
	case YAML_SCALAR_NODE:
		item->value_type = CONFIG_VALUE_SCALAR;
		item->value = (char *)node->data.scalar.value;
		break;

	case YAML_MAPPING_NODE:
		item->value_type = CONFIG_VALUE_MAP;
		item->values.node = node;
		config_map_reset(&item->values);
		break;

	case YAML_SEQUENCE_NODE:
		item->value_type = CONFIG_VALUE_LIST;
		item->list.node = node;
		config_list_reset(&item->list);
		break;

	default:
		DEBUG_MSG("Invalid item value type");
		return false;
	}
	return true;
}

void config_file_set_error(struct config_file *config, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	config->error = true;
	strbuf_vprintf_at(&config->error_str_buf, 0, msg, args);

	va_end(args);
}

char *config_file_get_error_string(struct config_file *config)
{
	return config->error_str_buf.str;
}

bool config_file_has_errors(struct config_file *config)
{
	return config->error;
}

struct config_file *config_file_open(char *filename)
{
	struct config_file *config;

	assert(filename != NULL);
	
	config = malloc(sizeof(struct config_file));
	if (config == NULL)
		goto error;

	/* TODO global evil */
	config_file = config;

	config->file = fopen(filename, "r");
	if (config->file == NULL) {
		return NULL;
	}

	yaml_parser_initialize(&config->parser);
	yaml_parser_set_input_file(&config->parser, config->file);

	yaml_parser_load(&config->parser, &config->doc);
	config->root_node = yaml_document_get_root_node(&config->doc);
	
	if (config->root_node == NULL) {
		config_file_set_error(config, "Syntax error: %s at %s:%u:%u\n",
			config->parser.problem,
			filename,
			config->parser.problem_mark.line + 1,
			config->parser.problem_mark.column + 1);
		goto error2;
	}

	config->error = false;
	strbuf_init(&config->error_str_buf);

	return config;

error2:
	yaml_document_delete(&config->doc);
	yaml_parser_delete(&config->parser);

	return config;

error:
	fclose(config->file);
	return NULL;
}

struct config_file *config_file_open_stdin()
{
	NOT_IMPLEMENTED;
}

bool config_file_get_root_map(struct config_file *config, struct config_map *map)
{
	assert(config != NULL);
	assert(map != NULL);

	map->node = config->root_node;
	config_map_reset(map);
	return true;
}

void config_file_close(struct config_file *config)
{
	assert(config != NULL);

	strbuf_free(&config->error_str_buf);
	yaml_document_delete(&config->doc);
	yaml_parser_delete(&config->parser);
	fclose(config->file);
}

bool config_map_next_item(struct config_map *map, struct config_item *item)
{
	yaml_node_t *key_node, *value_node;

	assert(map != NULL);
	assert(item != NULL);

	if (map->pair == map->node->data.mapping.pairs.top)
		return false;

	key_node = yaml_document_get_node(&config_file->doc, map->pair->key);
	value_node = yaml_document_get_node(&config_file->doc, map->pair->value);

	item->key = (char *)key_node->data.scalar.value;

	if(!load_config_item_value(item, value_node))
		return false;

	map->pair++;
	return true;
}

void config_map_reset(struct config_map *map)
{
	map->pair = map->node->data.mapping.pairs.start;
	map->num_items = (map->node->data.mapping.pairs.top
		- map->node->data.mapping.pairs.start);
}

size_t config_map_num_items(struct config_map *map)
{
	return map->num_items;
}

void config_list_reset(struct config_list *list)
{
	yaml_node_item_t *start = list->node->data.sequence.items.start;
	yaml_node_item_t *end = list->node->data.sequence.items.top;
	list->element = start;
	list->num_items = end - start;
}

bool config_list_next_item(struct config_list *list, struct config_item *item){
	yaml_node_t *value_node;

	assert(list != NULL);
	assert(item != NULL);

	if (list->element == list->node->data.sequence.items.top)
		return false;
	value_node = yaml_document_get_node(&config_file->doc, *list->element);
	if(!load_config_item_value(item, value_node))
		return false;

	list->element++;
	return true;
}

size_t config_list_num_items(struct config_list *list)
{
	return list->num_items;
}

bool config_list_get(struct config_list *list, size_t index, struct config_item *item)
{
	yaml_node_item_t val = list->node->data.sequence.items.start[index];
	yaml_node_t* node_val = yaml_document_get_node(&config_file->doc, val);

	item->key = NULL;
	return load_config_item_value(item, node_val);
}


bool config_map_get(struct config_map *map, const char *key, struct config_item *item)
{	
	config_map_reset(map);
	/* TODO O(n) time, maybe use a hashtable instead? */
	while (config_map_next_item(map, item)) {
		if (strcmp(item->key, key) == 0)
			return true;
	}

	return false;
}

bool convert_string_to_int(char *str, int *out)
{
	long ret;
	char *endptr;

	errno = 0;
	ret = strtol(str, &endptr, 10);

	if ((errno == ERANGE && (ret == LONG_MAX || ret == LONG_MIN))
		|| (errno != 0 && ret == 0) || endptr == str || *endptr != '\0') {
		config_file_set_error(config_file, "invalid int value: '%s'", str);
		return false;
	}

	*out = (int)ret;
	return true;
}

bool config_map_getopt(struct config_map *map, struct config_option *options)
{
	struct config_item item;
	struct config_option *opt;
	bool opt_found;

	for (opt = options; opt->name != NULL; opt++) {
		opt_found = false;
		config_map_reset(map);
		while (config_map_next_item(map, &item)) {
			if (strcmp(item.key, opt->name) == 0) {
				opt_found = true;
				break;
			}
		}

		if (!opt_found) {
			if (opt->required) {
				config_file_set_error(
					config_file,
					"missing required option '%s'",
					opt->name
				);

				return false;
			}
			continue;

		}

		switch (opt->type) {
		case CONFIG_OPTION_INT:
		case CONFIG_OPTION_STRING:
		case CONFIG_OPTION_MAC:
		case CONFIG_OPTION_BOOL:
			if (item.value_type != CONFIG_VALUE_SCALAR) {
				config_file_set_error(
					config_file,
					"value of '%s' has to be scalar",
					opt->name
				);
				return false;
			}
			break;
		}


		switch (opt->type) {
		case CONFIG_OPTION_INT:
			if (!convert_string_to_int(item.value, (int *)opt->ptr))
				return false;
			break;

		case CONFIG_OPTION_STRING:
			*((char **)opt->ptr) = item.value;
			break;

		case CONFIG_OPTION_BOOL:
			/* TODO: i think there is some YAML standard for this, find it */
			if(strcmp("true", item.value) == 0)
			   *((bool *)opt->ptr) = 1;
			if(strcmp("false", item.value) == 0)
			   *((bool *)opt->ptr) = 0;

			/* TODO implement this */
			break;

		case CONFIG_OPTION_MAC:
			/* TODO implement this */
			break;

		default:
			NOT_IMPLEMENTED;
		}
	}

	return true;
}

bool config_map_dispatch(struct config_map *map, const char *dispatch_key,
	struct config_action actions[], bool must_dispatch_all)
{
	struct config_item item, dispatch_item;
	struct config_action *action;
	bool dispatched;
	bool dispatch_ok;

	while (config_map_next_item(map, &item)) {
		if (item.value_type != CONFIG_VALUE_MAP) {
			/*
			 * TODO What is the correct behaviour here?
			 *
			 * Right now, I just skip an item which is not a map,
			 * because I cannot dispatch for it.
			 */ 
			continue;
		}

		if (!config_map_get(&item.values, dispatch_key, &dispatch_item)) {
			if (must_dispatch_all) {
				config_file_set_error(config_file, "missing dispatch key '%s'",
					dispatch_key);
				return false;
			}

			continue;
		}

		if (dispatch_item.value_type != CONFIG_VALUE_SCALAR) {
			config_file_set_error(config_file, "dispatch value must be scalar");
			return false;
		}

		dispatched = false;

		for (action = actions; action->keyword != NULL; action++) {
			if (strcmp(action->keyword, dispatch_item.value) == 0) {
				dispatch_ok = action->handler(&item, action->arg); 
				dispatched = true;
			}
		}

		if (!dispatched && must_dispatch_all) {
			config_file_set_error(config_file, "no action registred for value '%s'",
				dispatch_item.value);
			return false;
		}

		if (!dispatch_ok)
			return false;
	}

	return true;
}