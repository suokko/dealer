
#ifdef MVDEFAULT
#define CONCAT2(a,b) a ##_## b
#define CONCAT(a,b) CONCAT2(a,b)
#define DEFUN(x) CONCAT(MVDEFAULT, x)
#else
#define DEFUN(x) default_##x
#endif
namespace DEFUN() {
void test(unsigned);
}
