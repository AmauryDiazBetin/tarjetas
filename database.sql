-- Tabla de tarjetas autorizadas
CREATE TABLE IF NOT EXISTS tarjetas_autorizadas (
    uid VARCHAR(16) PRIMARY KEY,
    nombre VARCHAR(100) NOT NULL,
    tipo_acceso VARCHAR(20) NOT NULL,
    fecha_registro TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    activa BOOLEAN DEFAULT TRUE
);

-- Tabla de registros de acceso
CREATE TABLE IF NOT EXISTS registros_acceso (
    id SERIAL PRIMARY KEY,
    uid_tarjeta VARCHAR(16) NOT NULL,
    fecha_hora TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    autorizado BOOLEAN NOT NULL,
    nombre VARCHAR(100),
    tipo_acceso VARCHAR(20),
    FOREIGN KEY (uid_tarjeta) REFERENCES tarjetas_autorizadas(uid)
);

-- Datos iniciales (opcional)
INSERT INTO tarjetas_autorizadas (uid, nombre, tipo_acceso)
VALUES 
    ('9D49D005', 'Andres Valenzuela', 'admin'),
    ('C4C6AE05', 'Auder Gonzalez', 'empleado')
ON CONFLICT (uid) DO NOTHING;