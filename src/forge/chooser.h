#ifndef FORGE_CHOOSER_H_INCLUDED
#define FORGE_CHOOSER_H_INCLUDED

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parameter: choices   -> the choices to display
 * Parameter: choices_n -> the number of choices
 * Parameter: cpos      -> the starting row of the cursor
 * Returns: The index that they chose or -1 on failure/cancel
 * Description: Display all choices in a scrollable environment.
 *              When the user hits [enter], it will return that
 *              index, or -1 on failure/cancel.
 */
int forge_chooser(const char *msg, const char **choices, size_t choices_n, size_t cpos);

/**
 * Parameter: msg?      -> the message to display
 * Parameter: choices   -> the choices to display
 * Parameter: choices_n -> the number of choices
 * Parameter: cpos      -> the starting row of the cursor
 * Returns: The index that they chose or -1 on failure/cancel
 * Description: The same as forge_chooser() except it takes
 *              ownership of `choices`. It expects that each
 *              choice was individually malloc()'d, and the
 *              base pointer itself was also malloc()'d.
 */
int forge_chooser_take(const char *msg, char **choices, size_t choices_n, size_t cpos);

/**
 * Parameter: msg?    -> the message to display
 * Parameter: custom? -> the custom third option to choose
 * Returns: 1 on yes, 0 on no, 2 on custom (if available), and -1 on fail.
 * Description: Prompts the user for a yes/no option. If `custom`
 *              is not NULL, it will be presented as the third option.
 */
int forge_chooser_yesno(const char *msg, const char *custom, int cpos);

#ifdef __cplusplus
}
#endif

#endif // FORGE_CHOOSER_H_INCLUDED
