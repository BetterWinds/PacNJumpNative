// Runs the slow part of the Dreamcast build (PNG decode and VQ compression of one sprite sheet) off the page's main thread.
import { encodeSheet } from './dc_pack.js';
self.onmessage = async (e) => {
  const { id, png } = e.data;
  try { const o = await encodeSheet(png); self.postMessage({ id, o }, o ? [o.buffer] : []); }
  catch (err) { self.postMessage({ id, err: String(err && err.message || err) }); }
};
