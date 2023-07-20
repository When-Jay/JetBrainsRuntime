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
import sun.awt.datatransfer.ToolkitThreadBlockedHandler;

import java.awt.Image;
import java.awt.datatransfer.DataFlavor;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.HashMap;

public class WLDataTransferer extends DataTransferer {

    private final Map<String, Long> nameToLong = new HashMap<>();
    private final Map<Long, String> longToName = new HashMap<>();

    private static class HOLDER {
        static WLDataTransferer instance = new WLDataTransferer();
    }

    static WLDataTransferer getInstanceImpl() {
        return HOLDER.instance;
    }

    @Override
    public String getDefaultUnicodeEncoding() {
        return "iso-10646-ucs-2";
    }

    @Override
    public boolean isLocaleDependentTextFormat(long format) {
        return false;
    }

    @Override
    public boolean isFileFormat(long format) {
        // TODO
        String name = getNativeForFormat(format);
        System.out.println(name);
        try {
            var dataFlavor = new DataFlavor(name);
            System.out.println(dataFlavor.getPrimaryType());
            // TODO
            return "TODO".equals(dataFlavor.getPrimaryType());
        } catch (ClassNotFoundException e) {
            return false;
        }
    }

    @Override
    public boolean isImageFormat(long format) {
        // TODO: cache the result?
        String name = getNativeForFormat(format);
        System.out.println(name);
        List<DataFlavor> dataFlavor = SunClipboard.getDefaultFlavorTable().getFlavorsForNative(name);
        return dataFlavor.stream().anyMatch(df -> "image".equals(df.getPrimaryType()));
    }

    @Override
    protected Long getFormatForNativeAsLong(String formatName) {
        synchronized (this) {
            long nextID = nameToLong.size();
            Long thisID = nameToLong.putIfAbsent(formatName, nextID);
            if (thisID == null) {
                longToName.put(nextID, formatName);
            }
            return thisID;
        }
    }

    @Override
    protected String getNativeForFormat(long format) {
        synchronized (this) {
            return longToName.get(format);
        }
    }

    @Override
    protected ByteArrayOutputStream convertFileListToBytes(ArrayList<String> fileList) throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        for (int i = 0; i < fileList.size(); i++)
        {
            byte[] bytes = fileList.get(i).getBytes();
            if (i != 0) bos.write(0);
            bos.write(bytes, 0, bytes.length);
        }
        return bos;
    }

    @Override
    protected String[] dragQueryFile(byte[] bytes) {
        return new String[0];
    }

    @Override
    protected Image platformImageBytesToImage(byte[] bytes, long format) throws IOException {
        return null;
    }

    @Override
    protected byte[] imageToPlatformBytes(Image image, long format) throws IOException {
        return new byte[0];
    }

    @Override
    public ToolkitThreadBlockedHandler getToolkitThreadBlockedHandler() {
        return WLToolkitThreadBlockedHandler.getToolkitThreadBlockedHandler();
    }
}