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
#include "jni.h"

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

}

static void wl_offer(
        void *data,
        struct wl_data_offer *wl_data_offer,
        const char *mime_type)
{

}

static void wl_source_actions(
        void *data,
        struct wl_data_offer *wl_data_offer,
        uint32_t source_actions)
{

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
        return;
    }
}

static void wl_data_source_handle_send(
        void *data,
        struct wl_data_source *source,
        const char *mime_type,
        int fd)
{
    // TODO: call transferContentsWithType() from here
    close(fd);
}

static void wl_data_source_handle_cancelled(
        void *data,
        struct wl_data_source *source)
{
    // An application has replaced the clipboard contents
    wl_data_source_destroy(source);
}

static const struct wl_data_source_listener wl_data_source_listener = {
        .send = wl_data_source_handle_send,
        .cancelled = wl_data_source_handle_cancelled,
};

JNIEXPORT void JNICALL
Java_sun_awt_wl_WLClipboard_initNative(
        JNIEnv *env,
        jobject obj)
{
    // TODO: may be needed by DnD also, initialize in a common place
    wl_data_device = wl_data_device_manager_get_data_device(wl_ddm, wl_seat);
    wl_data_device_add_listener(wl_data_device, &wl_data_device_listener, NULL);

}

JNIEXPORT void JNICALL
Java_sun_awt_wl_WLClipboard_offerData(
        JNIEnv *env,
        jobject obj,
        jlong keyboardEnterSerial,
        jobjectArray mimeTypes,
        jobject content)
{
    struct wl_data_source *source = wl_data_device_manager_create_data_source(wl_ddm);
    wl_data_source_add_listener(source, &wl_data_source_listener, NULL);

    wl_data_source_offer(source, "text/plain");
    wl_data_source_offer(source, "text/html");

    wl_data_device_set_selection(wl_data_device, source, keyboardEnterSerial);
}
