## broken!

### Pleg does not work.

This is whilst drumlin undergoes a major refactoring,
but since we do not have a release,
that's fine.

Basically:
	Allocator -> drumlin::StripedAllocator
	Buffer -> own file
	Source -> drumlin::UsesStripedAllocator
	MockSource et al -> remain in source.h
	Relevance -> drumlin::HeapUseIdent
	Registry code will likely turn into functorials.

#### DO NOT USE

As if you wanted to.
