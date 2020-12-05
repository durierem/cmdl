/**
 * Interface pour une file syncrhonisée dont les objets sont stockés dans une
 * mémoire partagée.
 *
 * TODO: détails
 */
#ifndef SQUEUE__H
#define SQUEUE__H

#include <stdbool.h>

/**
 * Nombre maximum d'objets dans la file
 */
#define SQ_LENGTH_MAX 16

/**
 * Définit le type opaque SQueue pour l'utilisateur.
 */
typedef struct squeue * SQueue;

/**
 * Créé une nouvelle file synchronisée initialement vide.
 *
 * @param size La taille des objets de la file.
 * @return La file nouvellement créée.
 */
extern SQueue sq_empty(size_t size);

/**
 * Enfile un objet dans une file syncronisée.
 *
 * @param sq Une file synchronisée.
 * @param obj Un pointeur vers l'objet à enfiler.
 * @return Zéro en cas de succès, une valeur non nulle sinon. 
 */
extern int sq_enqueue(SQueue sq, const void *obj);

#endif
