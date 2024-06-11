#define ABSDIF(X, Y) ((X) > (Y) ? (X) - (Y) : (Y) - (X))
#define DISPLAY2(EXPR) printf(#EXPR "=%d\n", EXPR)
int main()
{
    DISPLAY2(ABSDIF(3, 8));
}