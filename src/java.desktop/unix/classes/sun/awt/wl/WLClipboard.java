/*
 * Copyright 2023 JetBrains s.r.o.
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
package sun.awt.wl;

import sun.awt.datatransfer.DataTransferer;
import sun.awt.datatransfer.SunClipboard;

import javax.swing.SwingUtilities;
import java.awt.datatransfer.DataFlavor;
import java.awt.datatransfer.FlavorTable;
import java.awt.datatransfer.Transferable;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Objects;
import java.util.SortedMap;

public final class WLClipboard extends SunClipboard {

    private final long ID;
    private final boolean isPrimary; // used from native

    private long clipboardNativePtr;     // guarded by 'this'
    private List<Long> clipboardFormats; // guarded by 'this'
    private int clipboardFD;             // guarded by 'this'

    static {
        initIDs();
        flavorTable = DataTransferer.adaptFlavorMap(getDefaultFlavorTable());
    }

    private final static FlavorTable flavorTable;

    public WLClipboard(String name, boolean isPrimary) {
        super(name);
        this.ID = initNative(isPrimary);
        this.isPrimary = isPrimary;
    }

    @Override
    public long getID() {
        return ID;
    }

    @Override
    protected void clearNativeContext() {
        long eventSerial = WLToolkit.getInputState().eventWithSerial().getSerial();
        synchronized (this) {
            cancelOffer(eventSerial);
        }
    }

    @Override
    protected void setContentsNative(Transferable contents) {
        // The server requires "serial number of the event that triggered this request"
        // as a proof of the right to copy data.
        long eventSerial = WLToolkit.getInputState().eventWithSerial().getSerial();
        if (eventSerial != 0) {
            WLDataTransferer wlDataTransferer = (WLDataTransferer) DataTransferer.getInstance();
            long[] formats = wlDataTransferer.getFormatsForTransferableAsArray(contents, flavorTable);

            if (formats.length > 0) {
                String[] mime = new String[formats.length];
                for (int i = 0; i < formats.length; i++) {
                    mime[i] = wlDataTransferer.getNativeForFormat(formats[i]);
                }

                offerData(eventSerial, mime, contents);
            }
        }
    }

    private void transferContentsWithType(Transferable contents, String mime, int destFD) throws IOException {
        // Called from native
        assert SwingUtilities.isEventDispatchThread();
        Objects.requireNonNull(contents);
        Objects.requireNonNull(mime);

        WLDataTransferer wlDataTransferer = (WLDataTransferer) DataTransferer.getInstance();
        SortedMap<Long,DataFlavor> formatMap =
                wlDataTransferer.getFormatsForTransferable(contents, flavorTable);

        long targetFormat = wlDataTransferer.getFormatForNativeAsLong(mime);
        DataFlavor flavor = formatMap.get(targetFormat);
        if (flavor != null) {
            FileDescriptor javaDestFD = new FileDescriptor();
            jdk.internal.access.SharedSecrets.getJavaIOFileDescriptorAccess().set(javaDestFD, destFD);

            try (var out = new FileOutputStream(javaDestFD)) {
                byte[] bytes = wlDataTransferer.translateTransferable(contents, flavor, targetFormat);
                // TODO: large data transfer will block EDT for a long time;
                //  implement an option to do the writing on a dedicated thread.
                out.write(bytes);
            }
        }
    }

    @Override
    protected long[] getClipboardFormats() {
        synchronized (this) {
            if (clipboardFormats != null && !clipboardFormats.isEmpty()) {
                long[] res = new long[clipboardFormats.size()];
                for (int i = 0; i < res.length; i++) {
                    res[i] = clipboardFormats.get(i);
                }
                return res;
            } else {
                return null;
            }
        }
    }

    @Override
    protected byte[] getClipboardData(long format) throws IOException {
        synchronized (this) {
            if (clipboardNativePtr != 0) {
                WLDataTransferer wlDataTransferer = (WLDataTransferer) DataTransferer.getInstance();
                String mime = wlDataTransferer.getNativeForFormat(format);
                int fd = requestDataInFormat(clipboardNativePtr, mime);
                FileDescriptor javaFD = new FileDescriptor();
                jdk.internal.access.SharedSecrets.getJavaIOFileDescriptorAccess().set(javaFD, fd);
                try (var in = new FileInputStream(javaFD)) {
                    byte[] bytes = in.readAllBytes();
                }
            }
        }
        return null;
    }

    private void handleClipboardFormat(long nativePtr, String mime) {
        // Called from native
        WLDataTransferer wlDataTransferer = (WLDataTransferer) DataTransferer.getInstance();
        Long format = wlDataTransferer.getFormatForNativeAsLong(mime);

        synchronized (this) {
            if (clipboardNativePtr != nativePtr) {
                clipboardFormats = new LinkedList<>();
                clipboardNativePtr = nativePtr;
            }
            clipboardFormats.add(format);
        }
    }



    @Override
    protected void registerClipboardViewerChecked() {
    }

    @Override
    protected void unregisterClipboardViewerChecked() {

    }

    private static native void initIDs();
    private native long initNative(boolean isPrimary);
    private native void offerData(long eventSerial, String[] mime, Object data);
    private native void cancelOffer(long eventSerial);

    private native int requestDataInFormat(long clipboardNativePtr, String mime);
}