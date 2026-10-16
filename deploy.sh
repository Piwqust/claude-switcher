#!/usr/bin/env bash
# Deploy the site to the shared server as static files behind its Caddy.
#
#   bash deploy.sh <domain>            ship the files (and add the Caddy site block the first time)
#
# Files go to /var/www/<domain>, swapped in atomically so a visitor never sees half a deploy.
# The Caddy block is appended once, validated, and reloaded; the previous Caddyfile is kept as
# /etc/caddy/Caddyfile.before-<domain>-<timestamp>. DNS (an A record to the server) is set in Cloudflare.
set -euo pipefail

DOMAIN="${1:?usage: bash deploy.sh <domain>}"
HOST="${DEPLOY_HOST:-144.31.223.224}"
PORT="${DEPLOY_PORT:-2222}"
KEY="${DEPLOY_KEY:-$HOME/.ssh/claude_root_ed25519}"
SSH=(ssh -o BatchMode=yes -o IdentitiesOnly=yes -i "$KEY" -p "$PORT" "root@$HOST")

cd "$(dirname "$0")"
FILES=(index.html styles.css app.js motion.js img)

echo "→ shipping ${FILES[*]} to /var/www/$DOMAIN"
tar -czf - "${FILES[@]}" | "${SSH[@]}" "set -e
  rm -rf '/var/www/$DOMAIN.new' && mkdir -p '/var/www/$DOMAIN.new'
  tar -xzf - -C '/var/www/$DOMAIN.new'
  chown -R root:root '/var/www/$DOMAIN.new' && find '/var/www/$DOMAIN.new' -type d -exec chmod 755 {} + && find '/var/www/$DOMAIN.new' -type f -exec chmod 644 {} +
  if [ -d '/var/www/$DOMAIN' ]; then mv '/var/www/$DOMAIN' '/var/www/$DOMAIN.old'; fi
  mv '/var/www/$DOMAIN.new' '/var/www/$DOMAIN'
  rm -rf '/var/www/$DOMAIN.old'"

echo "→ checking the Caddy site block"
sed "s/__DOMAIN__/$DOMAIN/g" caddy/site.caddy | "${SSH[@]}" "set -e
  if grep -q '^$DOMAIN {' /etc/caddy/Caddyfile; then echo '  already there'; exit 0; fi
  stamp=\$(date -u +%Y%m%dT%H%M%SZ)
  cp /etc/caddy/Caddyfile /etc/caddy/Caddyfile.before-$DOMAIN-\$stamp
  { echo; cat; } >> /etc/caddy/Caddyfile
  if caddy validate --config /etc/caddy/Caddyfile --adapter caddyfile >/dev/null 2>&1; then
    systemctl reload caddy && echo '  added and reloaded'
  else
    cp /etc/caddy/Caddyfile.before-$DOMAIN-\$stamp /etc/caddy/Caddyfile
    echo '  validation failed, Caddyfile restored' >&2; exit 1
  fi"

echo "→ done: https://$DOMAIN"
