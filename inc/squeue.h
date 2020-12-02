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
typedef struct squeue * sq;

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
 * @return Un pointeur vers l'objet enfilé en cas de succès, NULL sinon.
 */
extern const void *sq_enqueue(SQueue sq, const void *obj);

/**
 * Défile le prochain élément dans une file synchronisée.
 *
 * @param sq Une file synchronisée.
 * @return Un pointeur vers l'élément défilé.
 */
extern const void *sq_dequeue(SQueue sq);

/**
 * Teste si une file snchronisée donnée est vide.
 *
 * @param sq La file synchronisée à tester.
 * @return True si la file est vide, false sinon.
 */
extern bool sq_isempty(const SQueue sq);

/**
 * Teste si une file synchronisée donnée est pleine.
 *
 * @param sq La file synchronisée à tester.
 * @return True si la file est pleine, false sinon.
 */
extern bool sq_isfull(const SQueue sq);

/**
 * Renvoie le nombre d'éléments présents dans une file synchronisée.
 *
 * @param sq Une file synchronisée.
 * @return Le nombre d'élément dans la file donnée.
 */
extern size_t sq_size(const SQueue sq);

/**
 * Libère les ressources allouées pour une file synchronisée.
 *
 * @param sq Un pointeur vers la file à libérer.
 */
extern void sq_dispose(SQueue *sq);

#endif
