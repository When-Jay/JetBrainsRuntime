#ifndef JNIUTILITIES_H_
#define JNIUTILITIES_H_

#include "jni.h"
#include "jni_util.h"

#define LOG_NULL(dst_var, name) // TODO?

/********        GET CLASS SUPPORT    *********/

#define GET_CLASS(dst_var, cls) \
     if (dst_var == NULL) { \
         dst_var = (*env)->FindClass(env, cls); \
         if (dst_var != NULL) dst_var = (*env)->NewGlobalRef(env, dst_var); \
     } \
     LOG_NULL(dst_var, cls); \
     CHECK_NULL(dst_var);

#define DECLARE_CLASS(dst_var, cls) \
    static jclass dst_var = NULL; \
    GET_CLASS(dst_var, cls);

#define GET_CLASS_RETURN(dst_var, cls, ret) \
     if (dst_var == NULL) { \
         dst_var = (*env)->FindClass(env, cls); \
         if (dst_var != NULL) dst_var = (*env)->NewGlobalRef(env, dst_var); \
     } \
     LOG_NULL(dst_var, cls); \
     CHECK_NULL_RETURN(dst_var, ret);

#define DECLARE_CLASS_RETURN(dst_var, cls, ret) \
    static jclass dst_var = NULL; \
    GET_CLASS_RETURN(dst_var, cls, ret);


/********        GET METHOD SUPPORT    *********/

#define GET_METHOD(dst_var, cls, name, signature) \
     if (dst_var == NULL) { \
         dst_var = (*env)->GetMethodID(env, cls, name, signature); \
     } \
     LOG_NULL(dst_var, name); \
     CHECK_NULL(dst_var);

#define DECLARE_METHOD(dst_var, cls, name, signature) \
     static jmethodID dst_var = NULL; \
     GET_METHOD(dst_var, cls, name, signature);

#define GET_METHOD_RETURN(dst_var, cls, name, signature, ret) \
     if (dst_var == NULL) { \
         dst_var = (*env)->GetMethodID(env, cls, name, signature); \
     } \
     LOG_NULL(dst_var, name); \
     CHECK_NULL_RETURN(dst_var, ret);

#define DECLARE_METHOD_RETURN(dst_var, cls, name, signature, ret) \
     static jmethodID dst_var = NULL; \
     GET_METHOD_RETURN(dst_var, cls, name, signature, ret);

#define GET_STATIC_METHOD(dst_var, cls, name, signature) \
     if (dst_var == NULL) { \
         dst_var = (*env)->GetStaticMethodID(env, cls, name, signature); \
     } \
     LOG_NULL(dst_var, name); \
     CHECK_NULL(dst_var);

#define DECLARE_STATIC_METHOD(dst_var, cls, name, signature) \
     static jmethodID dst_var = NULL; \
     GET_STATIC_METHOD(dst_var, cls, name, signature);

#define GET_STATIC_METHOD_RETURN(dst_var, cls, name, signature, ret) \
     if (dst_var == NULL) { \
         dst_var = (*env)->GetStaticMethodID(env, cls, name, signature); \
     } \
     LOG_NULL(dst_var, name); \
     CHECK_NULL_RETURN(dst_var, ret);

#define DECLARE_STATIC_METHOD_RETURN(dst_var, cls, name, signature, ret) \
     static jmethodID dst_var = NULL; \
     GET_STATIC_METHOD_RETURN(dst_var, cls, name, signature, ret);

/********        GET FIELD SUPPORT    *********/


#define GET_FIELD(dst_var, cls, name, signature) \
     if (dst_var == NULL) { \
         dst_var = (*env)->GetFieldID(env, cls, name, signature); \
     } \
     LOG_NULL(dst_var, name); \
     CHECK_NULL(dst_var);

#define DECLARE_FIELD(dst_var, cls, name, signature) \
     static jfieldID dst_var = NULL; \
     GET_FIELD(dst_var, cls, name, signature);

#define GET_FIELD_RETURN(dst_var, cls, name, signature, ret) \
     if (dst_var == NULL) { \
         dst_var = (*env)->GetFieldID(env, cls, name, signature); \
     } \
     LOG_NULL(dst_var, name); \
     CHECK_NULL_RETURN(dst_var, ret);

#define DECLARE_FIELD_RETURN(dst_var, cls, name, signature, ret) \
     static jfieldID dst_var = NULL; \
     GET_FIELD_RETURN(dst_var, cls, name, signature, ret);

#define GET_STATIC_FIELD_RETURN(dst_var, cls, name, signature, ret) \
     if (dst_var == NULL) { \
         dst_var = (*env)->GetStaticFieldID(env, cls, name, signature); \
     } \
     LOG_NULL(dst_var, name); \
     CHECK_NULL_RETURN(dst_var, ret);

#define DECLARE_STATIC_FIELD_RETURN(dst_var, cls, name, signature, ret) \
     static jfieldID dst_var = NULL; \
     GET_STATIC_FIELD_RETURN(dst_var, cls, name, signature, ret);

/********        EXCEPTIONS SUPPORT    *********/

#define EXCEPTION_CLEAR(env) \
    if ((*env)->ExceptionCheck(env)) { \
        (*env)->ExceptionClear(env); \
    }

#endif // JNIUTILITIES_H_