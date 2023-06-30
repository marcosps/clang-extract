/* { dg-options "-DCE_EXTRACT_FUNCTIONS=Get_X -DCE_NO_EXTERNALIZATION" }*/
typedef struct Point {
  int x;
  int y;
} Point;

int Get_X(Point *p) {
  return p->x;
}


/* { dg-final { scan-tree-dump "typedef struct Point {\n *int x;\n *int y;\n} Point;" } } */
/* { dg-final { scan-tree-dump "int Get_X" } } */
