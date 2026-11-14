import { defineConfig } from 'astro/config';
import sitemap from '@astrojs/sitemap';

// Canonical, OG and the sitemap depend on `site`.
export default defineConfig({
  site: 'https://claude-switcher.angleford.co',
  trailingSlash: 'always',
  integrations: [
    sitemap({
      filter: (page) => !page.includes('/404'),
      i18n: { defaultLocale: 'en', locales: { en: 'en', ru: 'ru' } },
    }),
  ],
  prefetch: { prefetchAll: true, defaultStrategy: 'hover' },
  build: { inlineStylesheets: 'auto' },
});
