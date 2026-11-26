#ifndef APP_ASSERT_H
#define APP_ASSERT_H

/* A false condition makes the named type ill-formed at compile time. */
#define APP_STATIC_ASSERT(name, condition)                                     \
    typedef char name[(condition) ? 1 : -1]

#endif
