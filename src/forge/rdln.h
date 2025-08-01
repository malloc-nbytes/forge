#ifndef FORGE_RDLN_H_INCLUDED
#define FORGE_RDLN_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Parameter: prompt -> the prompt that prints
 * Returns: what the user typed, or NULL if cancelled
 * Description: Get input from the user similarly to the
 *              popular `readline` library. Allows for text
 *              navigation, backspace, delete, etc.
 */
char *forge_rdln(const char *prompt);

#ifdef __cplusplus
}
#endif

#endif // FORGE_RDLN_H_INCLUDED
