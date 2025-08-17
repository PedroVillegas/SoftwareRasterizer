
#ifndef GRACE_EXPORT_H
#define GRACE_EXPORT_H

#ifdef GRACE_STATIC_DEFINE
#  define GRACE_EXPORT
#  define GRACE_NO_EXPORT
#else
#  ifndef GRACE_EXPORT
#    ifdef Grace_EXPORTS
        /* We are building this library */
#      define GRACE_EXPORT __declspec(dllexport)
#    else
        /* We are using this library */
#      define GRACE_EXPORT __declspec(dllimport)
#    endif
#  endif

#  ifndef GRACE_NO_EXPORT
#    define GRACE_NO_EXPORT 
#  endif
#endif

#ifndef GRACE_DEPRECATED
#  define GRACE_DEPRECATED __declspec(deprecated)
#endif

#ifndef GRACE_DEPRECATED_EXPORT
#  define GRACE_DEPRECATED_EXPORT GRACE_EXPORT GRACE_DEPRECATED
#endif

#ifndef GRACE_DEPRECATED_NO_EXPORT
#  define GRACE_DEPRECATED_NO_EXPORT GRACE_NO_EXPORT GRACE_DEPRECATED
#endif

/* NOLINTNEXTLINE(readability-avoid-unconditional-preprocessor-if) */
#if 0 /* DEFINE_NO_DEPRECATED */
#  ifndef GRACE_NO_DEPRECATED
#    define GRACE_NO_DEPRECATED
#  endif
#endif

#endif /* GRACE_EXPORT_H */
