/*
 * Copyright (c) 2023, JetBrains s.r.o.. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "JNIUtilities.h"
#include "sun_awt_wl_WLClipboard.h"
#include "wayland-client-protocol.h"
#include "WLToolkit.h"

// A type both zwp_primary_selection_source_v1* and wl_data_source*
// are convertible to.
typedef void* data_source_t;

static jmethodID transferContentsWithTypeMID; // WLCipboard.transferContentsWithType()
static jfieldID  isPrimaryFID; // WLClipboard.isPrimary

typedef struct DataSourcePayload {
    jobject clipboard; // a global reference to WLClipboard
    jobject content;   // a global reference to Transferable
} DataSourcePayload;

static DataSourcePayload *
DataSourcePayload_Create(jobject clipboard, jobject content)
{
    DataSourcePayload * payload = malloc(sizeof(struct DataSourcePayload));
    if (payload) {
        payload->clipboard = clipboard;
        payload->content = content;
    }
    return payload;
}

static void
DataSourcePayload_Destroy(DataSourcePayload* payload)
{
    free(payload);
}

// Clipboard "devices", one for the actual clipboard and one for the selection clipboard.
// Implicitly assumed that WLClipboard can only create once instance of each.
static struct wl_data_device *wl_data_device;
static struct zwp_primary_selection_device_v1 *zwp_selection_device;

static void data_device_handle_data_offer(
        void *data,
        struct wl_data_device *data_device,
        struct wl_data_offer *offer);

static void data_device_handle_selection(
        void *data,
        struct wl_data_device *data_device,
        struct wl_data_offer *offer);

static const struct wl_data_device_listener wl_data_device_listener = {
        .data_offer = data_device_handle_data_offer,
        .selection = data_device_handle_selection,
};

static void
zwp_selection_offer(
        void *data,
        struct zwp_primary_selection_offer_v1 *zwp_primary_selection_offer_v1,
        const char *mime_type)
{
    printf("selection MIME type: %s\n", mime_type);
}

const struct zwp_primary_selection_offer_v1_listener zwp_selection_offer_listener = {
        .offer = zwp_selection_offer
};

static void
zwp_selection_device_handle_data_offer(
        void *data,
        struct zwp_primary_selection_device_v1 *device,
        struct zwp_primary_selection_offer_v1 *offer)
{
    zwp_primary_selection_offer_v1_add_listener(offer, &zwp_selection_offer_listener, NULL);
}

static void
zwp_selection_device_handle_selection(
        void *data,
        struct zwp_primary_selection_device_v1 *device,
        struct zwp_primary_selection_offer_v1 *offer)
{
    if (offer != NULL) {
        printf("Ready to receive selection\n");
    }
}

static const struct zwp_primary_selection_device_v1_listener zwp_selection_device_listener = {
        .data_offer = zwp_selection_device_handle_data_offer,
        .selection = zwp_selection_device_handle_selection
};

static void wl_action(
        void *data,
        struct wl_data_offer *wl_data_offer,
        uint32_t dnd_action)
{
    // TODO: this is for DnD
}

static void wl_offer(
        void *data,
        struct wl_data_offer *wl_data_offer,
        const char *mime_type)
{
    printf("wl_offer MIME type: %s\n", mime_type);
}

static void wl_source_actions(
        void *data,
        struct wl_data_offer *wl_data_offer,
        uint32_t source_actions)
{
    // TODO: this is for DnD
}

static const struct wl_data_offer_listener wl_data_offer_listener = {
        .action = wl_action,
        .offer = wl_offer,
        .source_actions = wl_source_actions
};

static void data_device_handle_data_offer(
        void *data,
        struct wl_data_device *data_device,
        struct wl_data_offer *offer)
{
    wl_data_offer_add_listener(offer, &wl_data_offer_listener, NULL);
}

static void data_device_handle_selection(
        void *data,
        struct wl_data_device *data_device,
        struct wl_data_offer *offer)
{
    // An application has set the clipboard contents
    if (offer == NULL) {
        printf("Clipboard is empty\n");
    } else {
        printf("Ready to receive clipboard\n");
    }
}

static void
SendClipboardToFD(DataSourcePayload *payload, const char *mime_type, int fd)
{
    JNIEnv *env = getEnv();
    jstring mime_type_string = (*env)->NewStringUTF(env, mime_type);
    if (payload->clipboard != NULL && payload->content != NULL && mime_type_string != NULL && fd >= 0) {
        (*env)->CallVoidMethod(env,
                               payload->clipboard,
                               transferContentsWithTypeMID,
                               payload->content,
                               mime_type_string,
                               fd);
        EXCEPTION_CLEAR(env);
    } else {
        // The file is normally closed on the Java side, so only close here
        // if the Java side wasn't involved.
        close(fd);
    }
    if (mime_type_string != NULL) {
        (*env)->DeleteLocalRef(env, mime_type_string);
    }
}

static void
CleanupClipboard(DataSourcePayload *payload)
{
    if (payload != NULL) {
        JNIEnv* env = getEnv();

        if (payload->clipboard != NULL) (*env)->DeleteGlobalRef(env, payload->clipboard);
        if (payload->content != NULL) (*env)->DeleteGlobalRef(env, payload->content);

        DataSourcePayload_Destroy(payload);
    }
}

static void wl_data_source_handle_send(
        void *data,
        struct wl_data_source *source,
        const char *mime_type,
        int fd)
{
    assert(data);
    SendClipboardToFD(data, mime_type, fd);
}

static void wl_data_source_handle_cancelled(
        void *data,
        struct wl_data_source *source)
{
    CleanupClipboard(data);
    wl_data_source_destroy(source);
}

static const struct wl_data_source_listener wl_data_source_listener = {
        .send = wl_data_source_handle_send,
        .cancelled = wl_data_source_handle_cancelled
};

static void
zwp_selection_source_handle_send(
        void *data,
        struct zwp_primary_selection_source_v1 *source,
        const char *mime_type,
        int32_t fd)
{
    DataSourcePayload * payload = data;
    assert(payload);

    SendClipboardToFD(payload, mime_type, fd);
}

void zwp_selection_source_handle_cancelled(
        void *data,
        struct zwp_primary_selection_source_v1 *source)
{
    CleanupClipboard(data);
    zwp_primary_selection_source_v1_destroy(source);
}

static const struct zwp_primary_selection_source_v1_listener zwp_selection_source_listener = {
        .send = zwp_selection_source_handle_send,
        .cancelled = zwp_selection_source_handle_cancelled
};

static jboolean
initJavaRefs(JNIEnv* env, jclass wlClipboardClass)
{
    GET_METHOD_RETURN(transferContentsWithTypeMID,
                      wlClipboardClass,
                      "transferContentsWithType",
                      "(Ljava/awt/datatransfer/Transferable;Ljava/lang/String;I)V",
                      JNI_FALSE);

    GET_FIELD_RETURN(isPrimaryFID,
                     wlClipboardClass,
                     "isPrimary",
                     "Z",
                     JNI_FALSE);

    return JNI_TRUE;
}

static jboolean
isPrimarySelectionClipboard(JNIEnv* env, jobject wlClipboard)
{
    return (*env)->GetBooleanField(env, wlClipboard, isPrimaryFID);
}

JNIEXPORT void JNICALL
Java_sun_awt_wl_WLClipboard_initIDs(
        JNIEnv *env,
        jclass wlClipboardClass)
{
    if (!initJavaRefs(env, wlClipboardClass)) {
        JNU_ThrowInternalError(env, "Failed to find WLClipboard members");
        return;
    }
}

JNIEXPORT jlong JNICALL
Java_sun_awt_wl_WLClipboard_initNative(
        JNIEnv *env,
        jobject obj,
        jboolean isPrimary)
{
    if (!isPrimary) {
        // TODO: may be needed by DnD also, initialize in a common place
        if (wl_data_device != NULL) {
            JNU_ThrowByName(env, "java/lang/IllegalStateException", "one data device has already been created");
            return 0;
        }
        wl_data_device = wl_data_device_manager_get_data_device(wl_ddm, wl_seat);
        wl_data_device_add_listener(wl_data_device, &wl_data_device_listener, NULL);
    } else {
        if (zwp_selection_dm != NULL) {
            if (zwp_selection_device != NULL) {
                JNU_ThrowByName(env,
                                "java/lang/IllegalStateException",
                                "one primary selection device has already been created");
                return 0;
            }
            zwp_selection_device = zwp_primary_selection_device_manager_v1_get_device(zwp_selection_dm, wl_seat);
            zwp_primary_selection_device_v1_add_listener(zwp_selection_device, &zwp_selection_device_listener, NULL);
        } else {
            JNU_ThrowByName(env,
                            "java/lang/UnsupportedOperationException",
                            "zwp_primary_selection_device_manager_v1 not available");
        }
    }

    return ptr_to_jlong(wl_data_device);
}

JNIEXPORT jlong JNICALL
Java_sun_awt_wl_WLClipboard_offerData(
        JNIEnv *env,
        jobject obj,
        jlong keyboardEnterSerial,
        jobjectArray mimeTypes,
        jobject content)
{
    jobject clipboardGlobalRef = (*env)->NewGlobalRef(env, obj); // deleted in wl_data_source_handle_cancelled()
    CHECK_NULL_RETURN(clipboardGlobalRef, 0);
    jobject contentGlobalRef = (*env)->NewGlobalRef(env, content); // deleted in wl_data_source_handle_cancelled()
    CHECK_NULL_RETURN(contentGlobalRef, 0);

    DataSourcePayload * payload = DataSourcePayload_Create(clipboardGlobalRef, contentGlobalRef);
    CHECK_NULL_THROW_OOME_RETURN(env, payload, "failed to allocate memory for DataSourcepayload", 0);

    const jboolean isPrimary = isPrimarySelectionClipboard(env, obj);
    data_source_t source = isPrimary
                    ? (data_source_t)zwp_primary_selection_device_manager_v1_create_source(zwp_selection_dm)
                    : (data_source_t)wl_data_device_manager_create_data_source(wl_ddm);

    if (source != NULL) {
        if (isPrimary) {
            zwp_primary_selection_source_v1_add_listener(
                    (struct zwp_primary_selection_source_v1 *)source,
                            &zwp_selection_source_listener,
                            payload);
        } else {
            wl_data_source_add_listener(
                    (struct wl_data_source *)source,
                            &wl_data_source_listener,
                            payload);
        }

        if (mimeTypes != NULL) {
            jint length = (*env)->GetArrayLength(env, mimeTypes);
            for (jint i = 0; i < length; i++) {
                jstring s = (*env)->GetObjectArrayElement(env, mimeTypes, i);
                const char *mimeType = (*env)->GetStringUTFChars(env, s, JNI_FALSE);
                if (isPrimary) {
                    zwp_primary_selection_source_v1_offer((struct zwp_primary_selection_source_v1 *)source, mimeType);
                } else {
                    wl_data_source_offer((struct wl_data_source *)source, mimeType);
                }
                (*env)->ReleaseStringUTFChars(env, s, mimeType);
                (*env)->DeleteLocalRef(env, s);
            }
        }

        if (isPrimary) {
            zwp_primary_selection_device_v1_set_selection(zwp_selection_device,
                                                          (struct zwp_primary_selection_source_v1 *)source,
                                                          keyboardEnterSerial);
        }
        else {
            wl_data_device_set_selection(wl_data_device,
                                         (struct wl_data_source *)source,
                                                 keyboardEnterSerial);
        }
    } else {
        // Failed to create a data source; give up and cleanup.
        (*env)->DeleteGlobalRef(env, payload->clipboard);
        (*env)->DeleteGlobalRef(env, payload->content);
        DataSourcePayload_Destroy(payload);
    }

    return ptr_to_jlong(source);
}

JNIEXPORT void JNICALL
Java_sun_awt_wl_WLClipboard_cancelOffer(
        JNIEnv *env,
        jobject obj,
        jlong keyboardEnterSerial)
{
    printf("cancelOffer\n");

    // This should automatically deliver the "cancelled" event where we clean up
    // both the previous source and the global reference to the transferable object.
    const jboolean isPrimary = isPrimarySelectionClipboard(env, obj);
    if (isPrimary) {
        zwp_primary_selection_device_v1_set_selection(zwp_selection_device, NULL, keyboardEnterSerial);
    } else {
        wl_data_device_set_selection(wl_data_device, NULL, keyboardEnterSerial);
    }
}