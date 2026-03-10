FROM node:20-alpine

WORKDIR /app

# Install deps first (better layer caching)
COPY package*.json ./
RUN npm ci --omit=dev

# Copy app
COPY . .

# If you’re using ES modules, keep your "type": "module" in package.json.
CMD ["node", "index.js"]