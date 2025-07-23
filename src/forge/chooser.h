#ifndef FORGE_CHOOSER_H_INCLUDED
#define FORGE_CHOOSER_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parameter: choices   -> the choices to display
 * Parameter: choices_n -> the number of choices
 * Returns: The index that they chose or -1 on failure/cancel
 * Description: Display all choices in a scrollable environment.
 *              When the user hits [enter], it will return that
 *              index, or -1 on failure/cancel.
 */
int forge_chooser(const char **choices, size_t choices_n);

/**
 * Parameter: choices   -> the choices to display
 * Parameter: choices_n -> the number of choices
 * Returns: The index that they chose or -1 on failure/cancel
 * Description: The same as forge_chooser() except it takes
 *              ownership of `choices`. It expects that each
 *              choice was individually malloc()'d, and the
 *              base pointer itself was also malloc()'d.
 */
int forge_chooser_take(char **choices, size_t choices_n);


#ifdef __cplusplus
}
#endif

#endif // FORGE_CHOOSER_H_INCLUDED
