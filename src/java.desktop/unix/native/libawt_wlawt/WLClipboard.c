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
#include "jni.h"
#include "jni_util.h"

#include "sun_awt_wl_WLClipboard.h"
#include "wayland-client-protocol.h"
#include "WLToolkit.h"

static struct wl_data_device *wl_data_device;

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

static void wl_data_source_handle_send(
        void *data,
        struct wl_data_source *source,
        const char *mime_type,
        int fd)
{
    // TODO: call transferContentsWithType() from here

    printf("Transferring clipboard content to fd=%d for mime_type=%s\n", fd, mime_type);
    // An application wants to paste the clipboard contents
    if (strcmp(mime_type, "text/plain;charset=utf-8") == 0) {
        write(fd, "hello from wayland", strlen("hello from wayland"));
    } else if (strcmp(mime_type, "text/html") == 0) {
        write(fd, "<b>hello from wayland</b>", strlen("<b>hello from wayland</b>"));
    } else {
        fprintf(stderr,
                "Destination client requested unsupported MIME type: %s\n",
                mime_type);
    }

    close(fd);
}

static void wl_data_source_handle_cancelled(
        void *data,
        struct wl_data_source *source)
{
    jobject content = (jobject)data;
    if (content != NULL) {
        JNIEnv* env = getEnv();
        (*env)->DeleteGlobalRef(env, content);
    }
    printf("Clipboard content cleared/replaced\n");
    // An application has replaced the clipboard contents
    wl_data_source_destroy(source);
}

static const struct wl_data_source_listener wl_data_source_listener = {
        .send = wl_data_source_handle_send,
        .cancelled = wl_data_source_handle_cancelled
};

JNIEXPORT jlong JNICALL
Java_sun_awt_wl_WLClipboard_initNative(
        JNIEnv *env,
        jobject obj,
        jboolean isPrimary)
{
    if (!isPrimary) {
        // TODO: may be needed by DnD also, initialize in a common place
        wl_data_device = wl_data_device_manager_get_data_device(wl_ddm, wl_seat);
        wl_data_device_add_listener(wl_data_device, &wl_data_device_listener, NULL);
    } else {
        // Use zwp_primary_selection_device_manager_v1 or throw UOE if unavailable
        JNU_ThrowByName(env,
                        "java/lang/UnsupportedOperationException",
                        "zwp_primary_selection_device_manager_v1 not available");
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
    struct wl_data_source *source = wl_data_device_manager_create_data_source(wl_ddm);
    if (source != NULL) {
        jobject contentGlobalRef = (*env)->NewGlobalRef(env, content);
        wl_data_source_add_listener(source, &wl_data_source_listener, (void*)contentGlobalRef);

        if (mimeTypes != NULL) {
            jint length = (*env)->GetArrayLength(env, mimeTypes);
            for (jint i = 0; i < length; i++) {
                jstring s = (*env)->GetObjectArrayElement(env, mimeTypes, i);
                const char *mimeType = (*env)->GetStringUTFChars(env, s, JNI_FALSE);
                wl_data_source_offer(source, mimeType);
                (*env)->ReleaseStringUTFChars(env, s, mimeType);
                (*env)->DeleteLocalRef(env, s);
            }
        }

        wl_data_device_set_selection(wl_data_device, source, keyboardEnterSerial);
    }

    return ptr_to_jlong(source);
}

JNIEXPORT void JNICALL
Java_sun_awt_wl_WLClipboard_cancelOffer(
        JNIEnv *env,
        jobject obj,
        jlong ptr)
{
    struct wl_data_source *source = jlong_to_ptr(ptr);
    if (source != NULL) {
        wl_data_source_destroy(source);
    }
}