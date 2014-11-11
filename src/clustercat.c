/** Induces word categories
 *  By Jon Dehdari, 2014
 *  Usage: ./clustercat [options] < input > output
**/

#include <limits.h>				// UCHAR_MAX, UINT_MAX
#include <time.h>				// clock_t, clock(), CLOCKS_PER_SEC

#include "clustercat.h"				// Model importing/exporting functions
#include "clustercat-data.h"
#include "clustercat-io.h"			// fill_sent_buffer()
#include "clustercat-math.h"			// get_decays(), dot_product()

#define USAGE_LEN 10000

// Declarations
void get_usage_string(char * restrict usage_string, int usage_len);
char * restrict class_algo = NULL;

struct_map *word_map       = NULL;	// Must initialize to NULL
struct_map *ngram_map      = NULL;	// Must initialize to NULL
struct_map *class_map      = NULL;	// Must initialize to NULL
struct_map_word_class *word2class_map = NULL;	// Must initialize to NULL
DECLARE_DATA_STRUCT_FLOAT; // for word_word_float_map
char usage[USAGE_LEN];
char * restrict class_file = NULL;


// Defaults
unsigned char ngram_order  = 2;
unsigned int min_count     = 0;
struct cmd_args cmd_args = {
	.class_algo             = EXCHANGE,
	.max_sents_in_buffer    = 512,
	.num_threads            = 6,
	.verbose                = 0,
};



int main(int argc, char **argv) {
	clock_t time_start = clock();
	argv_0_basename = basename(argv[0]);
	get_usage_string(usage, USAGE_LEN); // This is a big scary string, so build it elsewhere
	INIT_DATA_STRUCT_FLOAT;

	int arg_i;
	for (arg_i = 1; arg_i < argc; arg_i++) {
		if (!(strcmp(argv[arg_i], "-h") && strcmp(argv[arg_i], "--help"))) {
			printf("%s", usage);
			return 0;
		} else if (!(strcmp(argv[arg_i], "-j") && strcmp(argv[arg_i], "--jobs"))) {
			cmd_args.num_threads = (unsigned int) atol(argv[arg_i+1]);
			arg_i++;
		} else if (!strcmp(argv[arg_i], "--min-count")) {
			min_count = (unsigned int) atol(argv[arg_i+1]);
			arg_i++;
		} else if (!(strcmp(argv[arg_i], "-o") && strcmp(argv[arg_i], "--order"))) {
			ngram_order = (unsigned char) atoi(argv[arg_i+1]);
			arg_i++;
		} else if (!(strcmp(argv[arg_i], "-v") && strcmp(argv[arg_i], "--verbose"))) {
			cmd_args.verbose++;
		} else if (!strncmp(argv[arg_i], "-", 1)) { // Unknown flag
			printf("%s: Unknown command-line argument: %s\n\n", argv_0_basename, argv[arg_i]);
			printf("%s", usage);
			return -1;
	}
	//const char *file_name = argv[1];
	//printf("KEYLEN=%d,  sizeof(struct_map)=%lu\n", KEYLEN, sizeof(struct_map)); return 0;

	struct_model_metadata global_metadata;
	global_metadata.token_count = 0;
	global_metadata.line_count  = 0;


	// The list of unique words should always include <s>, unknown word, and </s>
	map_update_entry(&word_map, "<s>", 0);
	map_update_entry(&word_map, UNKNOWN_WORD, 0);
	map_update_entry(&word_map, "</s>", 0);

	char * restrict sent_buffer[cmd_args.max_sents_in_buffer];
	while (1) {
		// Fill sentence buffer
		long num_sents_in_buffer = fill_sent_buffer(stdin, sent_buffer, cmd_args.max_sents_in_buffer);
		if (num_sents_in_buffer == 0) // No more sentences in buffer
			break;

		global_metadata.line_count  += num_sents_in_buffer;
		global_metadata.token_count += process_sents_in_buffer(sent_buffer, num_sents_in_buffer);
	}


	clock_t time_model_built = clock();
	fprintf(stderr, "%s: Finished loading %lu tokens from %lu lines in %.2f secs\n", argv_0_basename, global_metadata.token_count, global_metadata.line_count, (double)(time_model_built - time_start)/CLOCKS_PER_SEC);
	unsigned long word_entries      = map_print_entries(&word_map, "", PRIMARY_SEP_CHAR, 0);
	unsigned long class_entries     = map_print_entries(&class_map, "#CL ", PRIMARY_SEP_CHAR, 0);
	unsigned long ngram_entries     = map_print_entries(&ngram_map, "#NG ", PRIMARY_SEP_CHAR, 0);
	unsigned long word_word_entries = 0;
	word_word_entries = PRINT_ENTRIES_FLOAT(DATA_STRUCT_FLOAT_ADDR DATA_STRUCT_FLOAT_NAME, "", PRIMARY_SEP_CHAR, 0);
	unsigned long total_entries = word_entries + class_entries + ngram_entries + word_word_entries;
	clock_t time_model_printed = clock();
	fprintf(stderr, "%s: Finished printing model in %.2f secs;", argv_0_basename, (double)(time_model_printed - time_model_built)/CLOCKS_PER_SEC);
	fprintf(stderr, "  %lu entries:  %lu types,  %lu class ngrams,  %lu word ngrams,  %lu word-words\n", total_entries, word_entries, class_entries, ngram_entries, word_word_entries);
	unsigned long map_entries = word_entries + class_entries + ngram_entries;
	fprintf(stderr, "%s: Approximate mem usage:  maps: %lu x %zu = %lu; word-word (using %s): %lu x %zu = %lu; total: %.1fMB\n", argv_0_basename, map_entries, sizeof(struct_map), sizeof(struct_map) * map_entries, DATA_STRUCT_FLOAT_HUMAN_NAME, word_word_entries, DATA_STRUCT_FLOAT_SIZE, DATA_STRUCT_FLOAT_SIZE * word_word_entries, (double)((sizeof(struct_map) * map_entries) + (DATA_STRUCT_FLOAT_SIZE * word_word_entries)) / 1048576);
	return 0;
}


void get_usage_string(char * restrict usage_string, int usage_len) {

	snprintf(usage_string, usage_len, "ClusterCat  (c) 2014 Jon Dehdari - LGPL v3 or Apache v2\n\
\n\
Usage:    clustercat [options] < input > output \n\
\n\
Function: Induces word categories from plaintext\n\
\n\
Options:\n\
     --class-algo <s>     Set class-induction algorithm {brown,exchange} (default: exchange)\n\
     --class-file <file>  Use word class tab-separated file (word\\tclass)\n\
     --class-order <i>    Maximum n-gram order to consider for class model (default: %d-grams)\n\
 -h, --help               Print this usage\n\
 -j, --jobs <i>           Set number of threads to run simultaneously (default: %d)\n\
     --min-count <i>      Minimum count of entries in training set to consider (default: %d)\n\
 -o, --order <i>          Maximum n-gram order in training set to consider for word n-gram model (default: %d-grams)\n\
 -v, --verbose            Print additional info to stderr.  Use additional -v for more verbosity\n\
\n\
", class_order, cmd_args.num_threads, min_count, ngram_order );
}


void increment_ngram(struct_map **ngram_map, char * restrict sent[const], const short * restrict word_lengths, short start_position, const sentlen_t i) {
	short j;
	size_t sizeof_char = sizeof(char); // We use this multiple times
	unsigned char ngram_len = 0; // Terminating char '\0' is same size as joining tab, so we'll just count that later

	// We first build the longest n-gram string, then successively remove the leftmost word

	//for (j = start_position; j <= i ; ++j) { // Determine length of longest n-gram string
	for (j = i; j >= start_position ; j--) { // Determine length of longest n-gram string, starting with smallest to ensure longest string is less than 255 chars
		if (ngram_len + sizeof_char + word_lengths[j]  < UCHAR_MAX ) { // Ensure n-gram string is less than 255 chars
			ngram_len += sizeof_char + word_lengths[j]; // the additional sizeof_char is for either a space for words in the history, or for a \0 for word_i
		} else { // It's too big; ensmallen n-gram
			start_position++;
		}

		//printf("increment_ngram1: start_position=%d, ngram_len=%u, j=%d, len(j)=%u, w_j=%s, i=%i, w_i=%s\n", start_position, ngram_len, j, word_lengths[j], sent[j], i, sent[i]);
	}

	if (!ngram_len) // We couldn't do anything with this n-gram because it was too long.  Wa, wa, wa
		return;

	char ngram[ngram_len];
	strcpy(ngram, sent[start_position]);
	// For single words, append \0 instead of space
	if (start_position < i)
		strcat(ngram, SECONDARY_SEP_STRING);
	else
		ngram[ngram_len] = '\0';
	//printf("increment_ngram1.5: start_position=%d, i=%i, w_i=%s, ngram_len=%d, ngram=<<%s>>\n", start_position, i, sent[i], ngram_len, ngram);

	for (j = start_position+1; j <= i ; ++j) { // Build longest n-gram string.  We do this as a separate loop than before since malloc'ing a bunch of times is probably more expensive than the first cheap loop
		strcat(ngram, sent[j]);
		if (j < i) // But wait! There's more!
			strcat(ngram, SECONDARY_SEP_STRING);
	}
	//printf("increment_ngram3: start_position=%d, i=%i, w_i=%s, ngram_len=%d, ngram=<<%s>>\n", start_position, i, sent[i], ngram_len, ngram);

	char * restrict jp = ngram;
	short diff = i - start_position;
	for (j = start_position; j <= i; ++j, --diff) { // Traverse longest n-gram string
		//if (cmd_args.verbose)
			//printf("increment_ngram4: start_position=%d, i=%i, w_i=%s, ngram_len=%d, ngram=<<%s>>, jp=<<%s>>\n", start_position, i, sent[i], ngram_len, ngram, jp);
		map_increment_entry(ngram_map, jp);
		//if (diff > 0) // 0 allows for unigrams
			jp += sizeof_char + word_lengths[j];
	}
}


unsigned long process_sents_in_buffer(char * restrict sent_buffer[], const long num_sents_in_buffer) {
	unsigned long token_count = 0;
	long current_sent_num;

	if (cmd_args.verbose > 0) // Precede program basename to verbose notices
		fprintf(stderr, "%s: L=lines; W=words\t", argv_0_basename);

	//#pragma omp parallel for private(current_sent_num) reduction(+:token_count) num_threads(cmd_args.num_threads) // static < dynamic < runtime <= auto < guided
	for (current_sent_num = 0; current_sent_num < num_sents_in_buffer; current_sent_num++) {
		token_count += process_sent(sent_buffer[current_sent_num]);
		if (cmd_args.verbose > 0 && (current_sent_num % 1000000 == 0) && (current_sent_num > 0)) {
			fprintf(stderr, "%liL/%luW ", current_sent_num, token_count); fflush(stderr);
		}
	}

	if (cmd_args.verbose > 0) { // Add final newline to verbose notices
		fprintf(stderr, "\n"); fflush(stderr);
	}

	return token_count;
}

unsigned long process_sent(char * restrict sent_str) {
	if (!strncmp(sent_str, "\n", 1)) // Ignore empty lines
		return 0;

	struct_sent_info sent_info;
	sent_info.sent       = (char **)malloc(STDIN_SENT_MAX_WORDS * sizeof(char*));
	sent_info.class_sent = (char **)calloc(STDIN_SENT_MAX_WORDS, sizeof(char*)); // We use calloc here to initialize values

	// We could have built up the word n-gram counts directly from sent_str, but it's
	// the only one out of the three models we're building that we can do this way, and
	// it's simpler to have a more uniform way of building these up.

	tokenize_sent(sent_str, &sent_info);
	unsigned long token_count = sent_info.length;

	// In the following loop we interpret i in two different ways.  For word/class n-gram models,
	// it's the right-most word in the n-gram. I wrote increment_ngram() earlier using the right-most interpretation of i.
	register sentlen_t i;
	for (i = 0; i < sent_info.length; i++) {
		map_increment_entry(&word_map, sent_info.sent[i]);

		if (ngram_order) {
			sentlen_t start_position_ngram = (i >= ngram_order-1) ? i - (ngram_order-1) : 0; // N-grams starting point is 0, for <s>
			increment_ngram(&ngram_map, sent_info.sent, sent_info.word_lengths, start_position_ngram, i);
		}

		if (class_order) {
			sentlen_t start_position_class = (i >= class_order-1) ? i - (class_order-1) : 0; // N-grams starting point is 0, for <s>
			increment_ngram(&class_map, sent_info.class_sent, sent_info.class_lengths, start_position_class, i);
		}
	}

	free_sent_info_local(sent_info);
	return token_count;
}


void tokenize_sent(char * restrict sent_str, struct_sent_info *sent_info) {

	// Initialize first element in sentence to <s>
	sent_info->sent[0] = "<s>";
	sent_info->class_sent[0] = "<s>";
	sent_info->word_lengths[0]  = strlen("<s>");
	sent_info->class_lengths[0] = strlen("<s>");

	sentlen_t w_i = 1; // Word 0 is <s>
	char * restrict pch;
	pch = strtok(sent_str, TOK_CHARS); // First token is treated differently with strtok

	for (; pch != NULL  &&  w_i < SENT_LEN_MAX; w_i++) {
		if (w_i == STDIN_SENT_MAX_WORDS - 1) { // Deal with pathologically-long lines
			fprintf(stderr, "%s: Warning: Truncating pathologically-long line starting with: %s %s %s %s %s %s ...\n", argv_0_basename, sent_info->sent[1], sent_info->sent[2], sent_info->sent[3], sent_info->sent[4], sent_info->sent[5], sent_info->sent[6]);
			break;
		}

		sent_info->sent[w_i] = pch;
		sent_info->word_lengths[w_i] = strlen(pch);

		if (sent_info->word_lengths[w_i] > MAX_WORD_LEN) { // Deal with pathologically-long words
			pch[MAX_WORD_LEN] = '\0';
			sent_info->word_lengths[w_i] = MAX_WORD_LEN;
			fprintf(stderr, "%s: Warning: Truncating pathologically-long word '%s'\n", argv_0_basename, pch);
		}

		if (class_file) {
			char * restrict class = get_class(&word2class_map, pch, UNKNOWN_WORD_CLASS);
			unsigned short class_len = strlen(class);
			sent_info->class_lengths[w_i] = class_len;
			sent_info->class_sent[w_i] = malloc(class_len + 1);
			strncpy(sent_info->class_sent[w_i], class, class_len+1);
		}

		pch = strtok(NULL, TOK_CHARS);
	}

	// Initialize last element in sentence to </s>
	sent_info->sent[w_i] = "</s>";
	sent_info->class_sent[w_i] = "</s>";
	sent_info->word_lengths[w_i]  = strlen("</s>");
	sent_info->class_lengths[w_i] = strlen("</s>");
	sent_info->length = w_i + 1; // Include <s>
}

// Slightly different from free_sent_info() since we don't free the individual words in sent_info.sent here
static inline void free_sent_info_local(struct_sent_info sent_info) {
	sentlen_t i = 1;
	for (; i < sent_info.length-1; ++i) // Assumes word_0 is <s> and word_sentlen is </s>, which weren't malloc'd
		free(sent_info.class_sent[i]);

	free(sent_info.sent);
	free(sent_info.class_sent);
}