/* { dg-options "-DCE_EXTRACT_FUNCTIONS=f" }*/
static int g()
{
  volatile int x = 3;
  return x;
}

int f(void)
{
  return g();
}

/* { dg-final { scan-tree-dump-not "static int g" } } */
/* { dg-final { scan-tree-dump "static int \(\*klp_g\)\(\);" } } */
/* { dg-final { scan-tree-dump "int f\(void\)\n{\n *return klp_g\(\)" } } */
