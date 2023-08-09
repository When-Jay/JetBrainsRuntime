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

import java.awt.datatransfer.DataFlavor;
import java.awt.datatransfer.Transferable;
import java.io.IOException;
import java.util.SortedMap;

public final class WLClipboard extends SunClipboard {

    private final long ID;
    private long nativeContext; // guarded by this

    public WLClipboard(String name, boolean isPrimary) {
        super(name);
        ID = initNative(isPrimary);
    }

    @Override
    public long getID() {
        return ID;
    }

    @Override
    protected void clearNativeContext() {
        synchronized (this) {
            cancelOffer(nativeContext);
        }
    }

    @Override
    protected void setContentsNative(Transferable contents) {
        long keyboardEnterSerial = WLToolkit.getInputState().keyboardEnterSerial();
        if (keyboardEnterSerial != 0) {
            WLDataTransferer wlDataTransferer = (WLDataTransferer) DataTransferer.getInstance();
            long[] formats = wlDataTransferer.getFormatsForTransferableAsArray
                    (contents, DataTransferer.adaptFlavorMap(getDefaultFlavorTable()));

            if (formats.length > 0) {
                String[] mime = new String[formats.length];
                for (int i = 0; i < formats.length; i++) {
                    mime[i] = wlDataTransferer.getNativeForFormat(formats[i]);
                }

                synchronized (this) {
                    nativeContext = offerData(keyboardEnterSerial, mime, contents);
                }
            }
        }
    }

    private int transferContentsWithType(Transferable contents, String mime) {
        // Called from native
        try {
            DataFlavor flavor = new DataFlavor(mime);
            if (contents.isDataFlavorSupported(flavor)) {
                SortedMap<Long,DataFlavor> formatMap =
                        DataTransferer.getInstance().getFormatsForTransferable
                                (contents, DataTransferer.adaptFlavorMap(getDefaultFlavorTable()));
                long format = 0; // TODO
                byte[] data = DataTransferer.getInstance().convertData(
                        this, contents, format, formatMap, WLToolkit.isToolkitThread());
                // TODO: Use DataTransferer.convertData(), maybe?
                // or  DataTransferer.getInstance().convertData() see XSelection.convertAndStore()
            }
        } catch (ClassNotFoundException | IOException e) {
            return -1;
        }

        return 0;
    }

    @Override
    protected long[] getClipboardFormats() {
        return new long[0];
    }

    @Override
    protected byte[] getClipboardData(long format) throws IOException {
        return new byte[0];
    }

    @Override
    protected void registerClipboardViewerChecked() {

    }

    @Override
    protected void unregisterClipboardViewerChecked() {

    }

    private native long initNative(boolean isPrimary);
    private native long offerData(long keyboardEnterSerial, String[] mime, Object data);
    private native void cancelOffer(long nativePtr);
}