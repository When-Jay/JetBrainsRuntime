/*
 * Copyright (c) 2003, 2015, Oracle and/or its affiliates. All rights reserved.
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

package sun.awt.X11;

import java.awt.AWTException;
import java.awt.Component;
import java.awt.Container;
import java.awt.geom.Point2D;
import java.awt.FontMetrics;
import java.awt.Point;
import java.awt.Rectangle;
import java.awt.im.spi.InputMethodContext;
import java.awt.peer.ComponentPeer;

import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.text.JTextComponent;

import sun.awt.AWTAccessor;
import sun.awt.X11InputMethod;

import sun.util.logging.PlatformLogger;

/**
 * Input Method Adapter for XIM (without Motif)
 *
 * @author JavaSoft International
 */
public class XInputMethod extends X11InputMethod {
    private static final PlatformLogger log = PlatformLogger.getLogger("sun.awt.X11.XInputMethod");

    public XInputMethod() throws AWTException {
        super();
    }

    public void setInputMethodContext(InputMethodContext context) {
        context.enableClientWindowNotification(this, true);
    }

    public void notifyClientWindowChange(Rectangle location) {
        XComponentPeer peer = (XComponentPeer)getPeer(clientComponentWindow);
        if (peer != null) {
            adjustStatusWindow(peer.getContentWindow());
        }
    }

    protected boolean openXIM() {
        return openXIMNative(XToolkit.getDisplay());
    }

    protected boolean createXIC() {
        XComponentPeer peer = (XComponentPeer)getPeer(clientComponentWindow);
        if (peer == null) {
            return false;
        }
        return createXICNative(peer.getContentWindow());
    }

    protected boolean recreateXIC(int ctxid) {
        final XComponentPeer peer = (XComponentPeer)getPeer(clientComponentWindow);
        if (peer == null || pData == 0)
            return true;
        return recreateXICNative(peer.getContentWindow(), pData, ctxid);
    }
    protected int releaseXIC() {
        if (pData == 0)
            return 0;
        return releaseXICNative(pData);
    }

    private static volatile long xicFocus = 0;

    /**
     * fix fcitx position
     */
    public void setXICTextCursorOffXY(ComponentPeer peer) {
        if (peer == null) {
            return;
        }
        xicFocus = ((XComponentPeer)peer).getContentWindow();
        int[] result = getOffXYRelateToFrame(peer ,true);
        setXICFocusNative(((XComponentPeer)peer).getContentWindow(),true,true,result);
    }

    protected void setXICFocus(ComponentPeer peer,
                                    boolean value, boolean active) {
        if (peer == null) {
            return;
        }
        xicFocus = ((XComponentPeer)peer).getContentWindow();
        int[] result = getOffXYRelateToFrame(peer ,value);
        setXICFocusNative(((XComponentPeer)peer).getContentWindow(),
                          value,
                          active,
                          result);
    }

    public static long getXICFocus() {
        return xicFocus;
    }

/* XAWT_HACK  FIX ME!
   do NOT call client code!
*/
    protected Container getParent(Component client) {
        return client.getParent();
    }

    /**
     * Returns peer of the given client component. If the given client component
     * doesn't have peer, peer of the native container of the client is returned.
     */
    protected ComponentPeer getPeer(Component client) {
        XComponentPeer peer;

        if (log.isLoggable(PlatformLogger.Level.FINE)) {
            log.fine("Client is " + client);
        }
        peer = (XComponentPeer)XToolkit.targetToPeer(client);
        while (client != null && peer == null) {
            client = getParent(client);
            peer = (XComponentPeer)XToolkit.targetToPeer(client);
        }
        if (log.isLoggable(PlatformLogger.Level.FINE)) {
            log.fine("Peer is {0}, client is {1}", peer, client);
        }

        if (peer != null)
            return peer;

        return null;
    }

    /*
     * Subclasses should override disposeImpl() instead of dispose(). Client
     * code should always invoke dispose(), never disposeImpl().
     */
    protected synchronized void disposeImpl() {
        super.disposeImpl();
        clientComponentWindow = null;
    }

    protected void awtLock() {
        XToolkit.awtLock();
    }

    protected void awtUnlock() {
        XToolkit.awtUnlock();
    }

    long getCurrentParentWindow() {
        XWindow peer = AWTAccessor.getComponentAccessor()
                                  .getPeer(clientComponentWindow);
        return peer.getContentWindow();
    }


    static void onXKeyEventFiltering(final boolean isXKeyEventFiltered) {
        // Fix of JBR-1573, JBR-2444, JBR-4394 (a.k.a. IDEA-246833).
        // Input method is considered broken if and only if all the following statements are true:
        //   * XFilterEvent have filtered more than filteredEventsThreshold last events of types KeyPress, KeyRelease;
        //   * Input method hasn't been changed (e.g. recreated);
        //   * The input context is not in preedit state (XNPreeditStartCallback has been called but then XNPreeditDoneCallback - hasn't)

        // The functionality is disabled
        if (BrokenImDetectionContext.EATEN_EVENTS_THRESHOLD < 1) {
            return;
        }
        // Must be called within AWT_LOCK
        if (!XToolkit.isAWTLockHeldByCurrentThread()) {
            return;
        }

        if (isXKeyEventFiltered) {
            final long nativeDataPtr = BrokenImDetectionContext.obtainCurrentXimNativeDataPtr();
            if (nativeDataPtr == 0) {
                ++BrokenImDetectionContext.eatenKeyEventsCount;
            } else {
                final int isDuringPreediting = BrokenImDetectionContext.isDuringPreediting(nativeDataPtr);
                if (isDuringPreediting > 0) {
                    BrokenImDetectionContext.eatenKeyEventsCount = 0;
                } else if (isDuringPreediting == 0) {
                    ++BrokenImDetectionContext.eatenKeyEventsCount;
                } else if (BrokenImDetectionContext.isCurrentXicPassive(nativeDataPtr)) {
                    // Unfortunately for passive XIC (XIMPreeditNothing | XIMStatusNothing) we have no way to get know
                    //  whether the XIC is in preediting state or not, so we won't handle this case.
                    BrokenImDetectionContext.eatenKeyEventsCount = 0;
                } else {
                    ++BrokenImDetectionContext.eatenKeyEventsCount;
                }
            }
        } else {
            BrokenImDetectionContext.eatenKeyEventsCount = 0;
        }

        if (BrokenImDetectionContext.eatenKeyEventsCount > BrokenImDetectionContext.EATEN_EVENTS_THRESHOLD) {
            BrokenImDetectionContext.eatenKeyEventsCount = 0;
            recreateAllXIC();
        }
    }

    private static class BrokenImDetectionContext {
        static final int EATEN_EVENTS_THRESHOLD;

        static int eatenKeyEventsCount = 0;

        /**
         * @return pointer to X11InputMethodData
         */
        static native long obtainCurrentXimNativeDataPtr();

        /**
         * <0 - unknown
         * >0 - true
         *  0 - false
         */
        static native int isDuringPreediting(long ximNativeDataPtr);

        static native boolean isCurrentXicPassive(long ximNativeDataPtr);


        static {
            int eatenEventsThresholdInitializer = 7;
            final String eventsThresholdMode = System.getProperty("recreate.x11.input.method", "true");

            if ("false".equals(eventsThresholdMode)) {
                eatenEventsThresholdInitializer = 0;
            } else if (!"true".equals(eventsThresholdMode)) {
                try {
                    eatenEventsThresholdInitializer = Integer.parseInt(eventsThresholdMode);
                } catch (NumberFormatException err) {
                    log.warning(
                        "Invalid value of \"recreate.x11.input.method\" system property \"" +
                            eventsThresholdMode +
                            "\". Only \"true\", \"false\" and integer values are supported",
                        err
                    );
                }
            }

            EATEN_EVENTS_THRESHOLD = eatenEventsThresholdInitializer;
        }
    }


    protected int[] getOffXYRelateToFrame(ComponentPeer peer, boolean value)  {
        int[] result = null;
        if(value && this.awtFocussedComponent !=null && this.awtFocussedComponent instanceof JTextComponent){
            try {
                Method method_getFontMetrics = null;
                Method method_getEditor = null;
                FontMetrics font_metrics = null;
                Object editor = null;
                int font_height = 0;
                int caret_x = 0;
                int caret_y = 0;
                if(this.awtFocussedComponent instanceof JTextArea || this.awtFocussedComponent instanceof JTextField){
                    method_getFontMetrics = this.awtFocussedComponent.getClass().getMethod("getFontMetrics");
                    font_metrics = (FontMetrics)method_getFontMetrics.invoke(this.awtFocussedComponent);
                    font_height = font_metrics.getHeight();
                    JTextComponent jc = (JTextComponent)this.awtFocussedComponent;
                    if(jc.getCaret().getMagicCaretPosition() != null) {
                        caret_x = jc.getCaret().getMagicCaretPosition().x;
                        caret_y = jc.getCaret().getMagicCaretPosition().y;
                    }
                } else {
                    method_getEditor = this.awtFocussedComponent.getClass().getMethod("getEditor");

                    editor = method_getEditor.invoke(this.awtFocussedComponent);

                    method_getFontMetrics = editor.getClass().getMethod("getFontMetrics",int.class);
                    font_metrics = (FontMetrics)method_getFontMetrics.invoke(editor, 0);
                    font_height = font_metrics.getHeight();
                    Method getCaretLocations = editor.getClass().getMethod("getCaretLocations", boolean.class);
                    Object[] locations = (Object[])getCaretLocations.invoke(editor, false);
                    Field point = locations[0].getClass().getField("myPoint");
                    Object myPoint = point.get(locations[0]);
                    if (myPoint instanceof Point) {
                        // convert to Point
                        Point p = (Point) myPoint;
                        caret_x = p.x;
                        caret_y = p.y;
                    } else if (myPoint instanceof Point2D) {
                        // convert to Point2D.Double
                        Point2D.Double pd = (Point2D.Double) myPoint;
                        caret_x = (int) pd.x;
                        caret_y = (int) pd.y;
                    }
                }

                Method method_getLocationOnScreen = this.awtFocussedComponent.getClass().getMethod("getLocationOnScreen");

                Point point = (Point)method_getLocationOnScreen.invoke(this.awtFocussedComponent);

                Method method_getNativeContainer = Component.class.getDeclaredMethod("getNativeContainer");
                method_getNativeContainer.setAccessible(true);
                Container c = (Container)method_getNativeContainer.invoke(awtFocussedComponent);
                if(c != null)
                    result = new int[]{point.x - c.getPeerLocationOnScreen().x + caret_x, point.y - c.getPeerLocationOnScreen().y + font_height + caret_y};

                return result;
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        return result;
    }

    /*
     * Native methods
     */
    private native boolean openXIMNative(long display);
    private native boolean createXICNative(long window);
    private native boolean recreateXICNative(long window, long px11data, int ctxid);
    private native int releaseXICNative(long px11data);
    private native void setXICFocusNative(long window,
                                    boolean value, boolean active, int[] offxy);
    private native void adjustStatusWindow(long window);
}
