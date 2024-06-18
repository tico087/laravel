# Use a imagem oficial do PHP com suporte ao Composer
FROM php:8.2-fpm

# Instalar dependências do sistema
RUN apt-get update && apt-get install -y \
    libzip-dev \
    unzip \
    git \
    curl

# Instalar extensões PHP necessárias
RUN docker-php-ext-install pdo pdo_mysql zip

# Instalar o Composer
COPY --from=composer:latest /usr/bin/composer /usr/bin/composer

# Configurar o diretório de trabalho
WORKDIR /var/www

# Copiar os arquivos do projeto para o contêiner
COPY . .

# Definir permissões corretas
RUN chown -R www-data:www-data /var/www

# Instalar dependências do Laravel
RUN composer install

# Expõe a porta que o Laravel usará
EXPOSE 8000
