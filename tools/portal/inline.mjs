// Post-build: inlines all JS/CSS chunks into a single self-contained index.html
// Usage: node inline.mjs
import fs from 'fs'
import path from 'path'
import { fileURLToPath } from 'url'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
const distDir = path.join(__dirname, 'dist', 'public')
const outFile = path.join(__dirname, 'esp32_portal.html')

let html = fs.readFileSync(path.join(distDir, 'index.html'), 'utf8')

// Inline all <link rel="stylesheet" href="..."> as <style>
html = html.replace(/<link rel="stylesheet" [^>]*href="([^"]+)"[^>]*\/?>/g, (match, href) => {
  const filePath = path.join(distDir, href.startsWith('/') ? href.slice(1) : href)
  if (!fs.existsSync(filePath)) { console.warn('CSS not found:', filePath); return '' }
  const css = fs.readFileSync(filePath, 'utf8')
  console.log('Inlining CSS:', href, `(${Math.round(css.length/1024)}KB)`)
  return `<style>${css}</style>`
})

// Inline all <script type="module" src="..."> as <script type="module">
html = html.replace(/<script ([^>]*?)src="([^"]+)"([^>]*?)><\/script>/g, (match, pre, href, post) => {
  const filePath = path.join(distDir, href.startsWith('/') ? href.slice(1) : href)
  if (!fs.existsSync(filePath)) { console.warn('JS not found:', filePath); return '' }
  const js = fs.readFileSync(filePath, 'utf8')
  console.log('Inlining JS:', href, `(${Math.round(js.length/1024)}KB)`)
  // Remove src, keep other attrs (like type="module")
  const attrs = (pre + post).trim()
  return `<script ${attrs}>${js}</script>`
})

// Remove any modulepreload link tags (they reference external files)
html = html.replace(/<link rel="modulepreload"[^>]*\/?>/g, '')
// Remove any prefetch/preload for JS files
html = html.replace(/<link rel="preload"[^>]*as="script"[^>]*\/?>/g, '')

// Update title
html = html.replace(/<title>[^<]*<\/title>/, '<title>GPO Phone Emulator</title>')

fs.writeFileSync(outFile, html, 'utf8')
const size = fs.statSync(outFile).size
console.log(`\nOutput: esp32_portal.html (${Math.round(size/1024)}KB)`)
console.log('Done.')
