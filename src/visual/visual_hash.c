
#include <string.h>
#include "visual_mod.h"

struct vis_Hash
{
	struct vis_HashNode **buckets;
	struct vis_HashNode **(*lookupfunc)(struct vis_Hash *hash,
		const char *key, TUINT *hvalp);
	TSIZE numnodes;
	TUINT numbuckets;
	TINT primeidx;
	TAPTR udata;
	struct TList *list;
};

static const TUINT hash_primes[] =
{
	11, 19, 37, 73, 109, 163, 251, 367, 557, 823, 1237, 1861, 2777,
 	4177, 6247, 9371, 14057, 21089, 31627, 47431, 71143, 106721, 160073,
 	240101, 360163, 540217, 810343, 1215497, 1823231, 2734867, 4102283,
 	6153409, 9230113, 13845163
};

#define HASH_NUMPRIMES	(sizeof(hash_primes) / sizeof(unsigned int))
#define HASH_MINLOAD	0
#define HASH_MAXLOAD	3
#define HASH_MINSIZE	11
#define HASH_MAXSIZE	13845163

static void
hash_resize(struct TVisualBase *mod, struct vis_Hash *hash)
{
	struct TExecBase *TExecBase = TGetExecBase(mod);
	TINT numbuckets = hash->numbuckets;
	TSIZE load = hash->numnodes / numbuckets;
	TINT pi = hash->primeidx;

	if (numbuckets < HASH_MAXSIZE && load >= HASH_MAXLOAD)
		for (; pi < (TINT) HASH_NUMPRIMES - 1 &&
			hash_primes[pi + 1] < hash->numnodes; ++pi);
	else if (numbuckets > HASH_MINSIZE && load <= HASH_MINLOAD)
		for (; pi >= 0 && hash_primes[pi] >= hash->numnodes; --pi);

	if (pi != hash->primeidx)
	{
		struct vis_HashNode **newbuckets;
		int newnumbuckets = hash_primes[pi];
		newbuckets = TAlloc0(mod->vis_MemMgr,
			sizeof(struct vis_HashNode) * newnumbuckets);
		if (newbuckets)
		{
			TUINT i;
			for (i = 0; i < hash->numbuckets; ++i)
			{
				struct vis_HashNode *next, *node = hash->buckets[i];
				while (node)
				{
					int hashidx = node->hash % newnumbuckets;
					next = (struct vis_HashNode *) node->node.tln_Succ;
					node->node.tln_Succ = (struct TNode *) newbuckets[hashidx];
					newbuckets[hashidx] = node;
					node = next;
				}
			}

			TFree(hash->buckets);
			hash->buckets = newbuckets;
			hash->numbuckets = newnumbuckets;
			hash->primeidx = pi;
		}
	}
}

static struct vis_HashNode **
hash_lookupstring(struct vis_Hash *hash, const char *key, TUINT *hvalp)
{
	struct vis_HashNode **bucket;
	TUINT hval = 0;
	char *s = (char *) key;
	TUINT g;
	int c;

	while ((c = *s++))
	{
		hval = (hval << 4) + c;
		if ((g = hval & 0xf0000000))
		{
			hval ^= g >> 24;
			hval ^= g;
		}
	}

	bucket = &hash->buckets[hval % hash->numbuckets];
	while (*bucket)
	{
		if (strcmp((char *) (*bucket)->key, (char *) key) == 0)
			break;
		bucket = (TAPTR) &(*bucket)->node.tln_Succ;
	}

	*hvalp = hval;
	return bucket;
}

/*****************************************************************************/

LOCAL int vis_puthash(struct TVisualBase *mod, struct vis_Hash *hash,
	const TSTRPTR key, TTAG value)
{
	struct TExecBase *TExecBase = TGetExecBase(mod);
	struct vis_HashNode **bucket, *newnode;
	TUINT hval;

	bucket = (*hash->lookupfunc)(hash, key, &hval);
	if (*bucket)
	{
		/* key exists - overwrite */
		(*bucket)->value = value;
		return 1;
	}

	/* key does not exist - create new node */
	newnode = TAlloc(mod->vis_MemMgr,
		sizeof(struct vis_HashNode));
	if (newnode)
	{
		size_t len = strlen(key) + 1;
		char *newkey = TAlloc(mod->vis_MemMgr, len);
		if (newkey)
		{
			memcpy(newkey, key, len);
			newnode->node.tln_Succ = TNULL;
			newnode->key = newkey;
			newnode->value = value;
			newnode->hash = hval;
			*bucket = newnode;
			hash->numnodes++;
			hash_resize(mod, hash);
			return 1;
		}
		TFree(newnode);
	}

	return 0;
}

LOCAL int vis_gethash(struct TVisualBase *mod, struct vis_Hash *hash,
	const TSTRPTR key, TTAG *valp)
{
	TUINT hval;
	struct vis_HashNode **bucket = (*hash->lookupfunc)(hash, key, &hval);
	if (*bucket)
	{
		if (valp)
			*valp = (*bucket)->value;
		return 1;
	}
	return 0;
}

LOCAL int vis_remhash(struct TVisualBase *mod, struct vis_Hash *hash,
	const TSTRPTR key)
{
	struct TExecBase *TExecBase = TGetExecBase(mod);
	TUINT hval;
	struct vis_HashNode **bucket = (*hash->lookupfunc)(hash, key, &hval);
	if (*bucket)
	{
		struct vis_HashNode *node = *bucket;
		*bucket = (struct vis_HashNode *) node->node.tln_Succ;
		TFree((void *) node->key);
		TFree(node);
		hash->numnodes--;
		hash_resize(mod, hash);
		return 1;
	}
	return 0;
}

LOCAL struct vis_Hash *vis_createhash(struct TVisualBase *mod, void *udata)
{
	struct TExecBase *TExecBase = TGetExecBase(mod);
	struct vis_Hash *hash = TAlloc(mod->vis_MemMgr, sizeof(struct vis_Hash));
	if (hash)
	{
		hash->udata = udata;
		hash->buckets = TAlloc0(mod->vis_MemMgr,
			sizeof(struct vis_Hash) * HASH_MINSIZE);
		if (hash->buckets)
		{
			hash->numbuckets = HASH_MINSIZE;
			hash->lookupfunc = hash_lookupstring;
			hash->numnodes = 0;
			hash->primeidx = 0;
			hash->list = TNULL;
			return hash;
		}
		TFree(hash);
	}
	return TNULL;
}

LOCAL void vis_destroyhash(struct TVisualBase *mod, struct vis_Hash *hash)
{
	struct TExecBase *TExecBase = TGetExecBase(mod);
	if (hash)
	{
		TUINT i;
		vis_hashunlist(mod, hash);
		for (i = 0; i < hash->numbuckets; ++i)
		{
			struct vis_HashNode *node, *nnode = hash->buckets[i];
			while ((node = nnode))
			{
				nnode = (struct vis_HashNode *) node->node.tln_Succ;
				if (node)
				{
					TFree(node->key);
					TFree(node);
				}
			}
		}
		TFree(hash->buckets);
		TFree(hash);
	}
}

LOCAL TUINT
vis_hashtolist(struct TVisualBase *mod, struct vis_Hash *hash,
	struct TList *list)
{
	if (list && hash->numnodes > 0)
	{
		TUINT i;
		for (i = 0; i < hash->numbuckets; ++i)
		{
			struct TNode *node, *next;
			for (node = &hash->buckets[i]->node; (node); node = next)
			{
				next = node->tln_Succ;
				TAddTail(list, node);
			}
		}
		hash->list = list;
		return hash->numnodes;
	}
	return 0;
}

LOCAL void
vis_hashunlist(struct TVisualBase *mod, struct vis_Hash *hash)
{
	if (hash->list)
	{
		TUINT i;
		struct vis_HashNode **bucket;
		struct TNode *nextnode, *node = hash->list->tlh_Head;
		for (i = 0; i < hash->numbuckets; ++i)
			hash->buckets[i] = TNULL;
		while ((nextnode = node->tln_Succ))
		{
			TRemove(node);
			bucket = &hash->buckets[((struct vis_HashNode *)node)->hash %
				hash->numbuckets];
			node->tln_Succ = (struct TNode *) *bucket;
			*bucket = (struct vis_HashNode *) node;
			node = nextnode;
		}
		hash->list = TNULL;
	}
}
