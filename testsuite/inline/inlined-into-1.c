/* { dg-compile "-fdump-ipa-clones -O3 -g3 -shared"} */
/* { dg-options "-where-is-inlined g"} */

static inline int g(void)
{
  return 42;
}

static __attribute__((noinline)) h(void)
{
  return g();
}

int f()
{
  return h() + g();
}

int main(void)
{
  return f() + g();
}


/* { dg-final { scan-tree-dump "f" } } */
/* { dg-final { scan-tree-dump "g" } } */
/* { dg-final { scan-tree-dump "f *.*FUNC\tPublic symbol\n" } } */
/* { dg-final { scan-tree-dump "h *.*FUNC\tPrivate symbol\n" } } */
/* { dg-final { scan-tree-dump "main *.*FUNC\tPublic symbol\n" } } */
