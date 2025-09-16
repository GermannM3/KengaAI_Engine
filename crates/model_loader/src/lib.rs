use anyhow::{Result, Context};
use glam::{Vec3, Vec2};
use std::collections::HashMap;
use std::fs;
use std::path::Path;

/// 3D Vertex structure
#[derive(Debug, Clone)]
pub struct Vertex {
    pub position: Vec3,
    pub normal: Vec3,
    pub tex_coords: Vec2,
}

impl Default for Vertex {
    fn default() -> Self {
        Self {
            position: Vec3::ZERO,
            normal: Vec3::Z,
            tex_coords: Vec2::ZERO,
        }
    }
}

/// Material structure
#[derive(Debug, Clone)]
pub struct Material {
    pub name: String,
    pub diffuse_color: Vec3,
    pub specular_color: Vec3,
    pub shininess: f32,
    pub diffuse_texture: Option<String>,
}

/// 3D Mesh structure
#[derive(Debug, Clone)]
pub struct Mesh {
    pub name: String,
    pub vertices: Vec<Vertex>,
    pub indices: Vec<u32>,
    pub material: Option<Material>,
}

/// 3D Model structure
#[derive(Debug, Clone)]
pub struct Model {
    pub name: String,
    pub meshes: Vec<Mesh>,
    pub materials: HashMap<String, Material>,
}

/// OBJ file loader
pub struct ObjLoader;

impl ObjLoader {
    /// Load OBJ file from path
    pub fn load<P: AsRef<Path>>(path: P) -> Result<Model> {
        let path = path.as_ref();
        let content = fs::read_to_string(path)
            .with_context(|| format!("Failed to read OBJ file: {}", path.display()))?;

        let mut model = Model {
            name: path.file_stem()
                .and_then(|s| s.to_str())
                .unwrap_or("unnamed")
                .to_string(),
            meshes: Vec::new(),
            materials: HashMap::new(),
        };

        Self::parse_obj(&content, &mut model, path)?;

        Ok(model)
    }

    /// Parse OBJ file content
    fn parse_obj(content: &str, model: &mut Model, obj_path: &Path) -> Result<()> {
        let mut positions: Vec<Vec3> = Vec::new();
        let mut normals: Vec<Vec3> = Vec::new();
        let mut tex_coords: Vec<Vec2> = Vec::new();
        let mut current_mesh: Option<Mesh> = None;
        let mut current_material = None;

        for (_line_num, line) in content.lines().enumerate() {
            let line = line.trim();

            if line.is_empty() || line.starts_with('#') {
                continue;
            }

            let parts: Vec<&str> = line.split_whitespace().collect();
            if parts.is_empty() {
                continue;
            }

            match parts[0] {
                "mtllib" => {
                    // Load material library
                    if parts.len() > 1 {
                        let mtl_path = obj_path.parent()
                            .unwrap_or(Path::new("."))
                            .join(parts[1]);
                        Self::load_mtl(&mtl_path, &mut model.materials)?;
                    }
                }

                "usemtl" => {
                    // Use material
                    if parts.len() > 1 {
                        current_material = Some(parts[1].to_string());
                    }
                }

                "o" => {
                    // New object
                    if let Some(mesh) = current_mesh.take() {
                        model.meshes.push(mesh);
                    }

                    let name = if parts.len() > 1 {
                        parts[1].to_string()
                    } else {
                        format!("object_{}", model.meshes.len())
                    };

                    current_mesh = Some(Mesh {
                        name,
                        vertices: Vec::new(),
                        indices: Vec::new(),
                        material: None,
                    });
                }

                "v" => {
                    // Vertex position
                    if parts.len() >= 4 {
                        let x = parts[1].parse::<f32>()?;
                        let y = parts[2].parse::<f32>()?;
                        let z = parts[3].parse::<f32>()?;
                        positions.push(Vec3::new(x, y, z));
                    }
                }

                "vn" => {
                    // Vertex normal
                    if parts.len() >= 4 {
                        let x = parts[1].parse::<f32>()?;
                        let y = parts[2].parse::<f32>()?;
                        let z = parts[3].parse::<f32>()?;
                        normals.push(Vec3::new(x, y, z));
                    }
                }

                "vt" => {
                    // Texture coordinates
                    if parts.len() >= 3 {
                        let u = parts[1].parse::<f32>()?;
                        let v = parts[2].parse::<f32>()?;
                        tex_coords.push(Vec2::new(u, v));
                    }
                }

                "f" => {
                    // Face
                    if let Some(ref mut mesh) = current_mesh {
                        Self::parse_face(&parts[1..], mesh, &positions, &normals, &tex_coords)?;
                    }
                }

                _ => {
                    // Ignore other directives for now
                }
            }
        }

        // Add final mesh
        if let Some(mesh) = current_mesh.take() {
            model.meshes.push(mesh);
        }

        // Assign materials to meshes
        for mesh in &mut model.meshes {
            if let Some(ref mat_name) = current_material {
                if let Some(material) = model.materials.get(mat_name) {
                    mesh.material = Some(material.clone());
                }
            }
        }

        Ok(())
    }

    /// Parse face definition
    fn parse_face(
        face_parts: &[&str],
        mesh: &mut Mesh,
        positions: &[Vec3],
        normals: &[Vec3],
        tex_coords: &[Vec2],
    ) -> Result<()> {
        if face_parts.len() < 3 {
            return Ok(()); // Skip invalid faces
        }

        let mut face_indices = Vec::new();

        for part in face_parts {
            let indices: Vec<&str> = part.split('/').collect();

            let vertex_idx = if !indices[0].is_empty() {
                let idx = indices[0].parse::<i32>()?;
                if idx < 0 {
                    (positions.len() as i32 + idx) as usize
                } else {
                    (idx - 1) as usize
                }
            } else {
                0
            };

            let tex_idx = if indices.len() > 1 && !indices[1].is_empty() {
                let idx = indices[1].parse::<i32>()?;
                if idx < 0 {
                    (tex_coords.len() as i32 + idx) as usize
                } else {
                    (idx - 1) as usize
                }
            } else {
                0
            };

            let normal_idx = if indices.len() > 2 && !indices[2].is_empty() {
                let idx = indices[2].parse::<i32>()?;
                if idx < 0 {
                    (normals.len() as i32 + idx) as usize
                } else {
                    (idx - 1) as usize
                }
            } else {
                0
            };

            // Create vertex
            let vertex = Vertex {
                position: positions.get(vertex_idx).copied().unwrap_or(Vec3::ZERO),
                tex_coords: tex_coords.get(tex_idx).copied().unwrap_or(Vec2::ZERO),
                normal: normals.get(normal_idx).copied().unwrap_or(Vec3::Z),
            };

            let vertex_index = mesh.vertices.len() as u32;
            mesh.vertices.push(vertex);
            face_indices.push(vertex_index);
        }

        // Triangulate face (simple fan triangulation)
        if face_indices.len() >= 3 {
            for i in 1..face_indices.len() - 1 {
                mesh.indices.push(face_indices[0]);
                mesh.indices.push(face_indices[i]);
                mesh.indices.push(face_indices[i + 1]);
            }
        }

        Ok(())
    }

    /// Load MTL material file
    fn load_mtl(path: &Path, materials: &mut HashMap<String, Material>) -> Result<()> {
        if !path.exists() {
            return Ok(()); // MTL file is optional
        }

        let content = fs::read_to_string(path)
            .with_context(|| format!("Failed to read MTL file: {}", path.display()))?;

        let mut current_material: Option<Material> = None;

        for line in content.lines() {
            let line = line.trim();

            if line.is_empty() || line.starts_with('#') {
                continue;
            }

            let parts: Vec<&str> = line.split_whitespace().collect();
            if parts.is_empty() {
                continue;
            }

            match parts[0] {
                "newmtl" => {
                    // Save previous material
                    if let Some(mat) = current_material.take() {
                        materials.insert(mat.name.clone(), mat);
                    }

                    // Create new material
                    let name = parts.get(1).unwrap_or(&"unnamed").to_string();
                    current_material = Some(Material {
                        name,
                        diffuse_color: Vec3::ONE,
                        specular_color: Vec3::ZERO,
                        shininess: 1.0,
                        diffuse_texture: None,
                    });
                }

                "Kd" => {
                    // Diffuse color
                    if let Some(ref mut mat) = current_material {
                        if parts.len() >= 4 {
                            mat.diffuse_color = Vec3::new(
                                parts[1].parse().unwrap_or(1.0),
                                parts[2].parse().unwrap_or(1.0),
                                parts[3].parse().unwrap_or(1.0),
                            );
                        }
                    }
                }

                "Ks" => {
                    // Specular color
                    if let Some(ref mut mat) = current_material {
                        if parts.len() >= 4 {
                            mat.specular_color = Vec3::new(
                                parts[1].parse().unwrap_or(0.0),
                                parts[2].parse().unwrap_or(0.0),
                                parts[3].parse().unwrap_or(0.0),
                            );
                        }
                    }
                }

                "Ns" => {
                    // Shininess
                    if let Some(ref mut mat) = current_material {
                        if parts.len() >= 2 {
                            mat.shininess = parts[1].parse().unwrap_or(1.0);
                        }
                    }
                }

                "map_Kd" => {
                    // Diffuse texture
                    if let Some(ref mut mat) = current_material {
                        if parts.len() >= 2 {
                            mat.diffuse_texture = Some(parts[1].to_string());
                        }
                    }
                }

                _ => {
                    // Ignore other material properties
                }
            }
        }

        // Save final material
        if let Some(mat) = current_material.take() {
            materials.insert(mat.name.clone(), mat);
        }

        Ok(())
    }
}

/// Convert model to renderable format
pub fn model_to_render_data(model: &Model) -> (Vec<Vertex>, Vec<u32>) {
    let mut all_vertices = Vec::new();
    let mut all_indices = Vec::new();
    let mut vertex_offset = 0;

    for mesh in &model.meshes {
        // Add vertices
        all_vertices.extend_from_slice(&mesh.vertices);

        // Add indices with offset
        for &index in &mesh.indices {
            all_indices.push(index + vertex_offset);
        }

        vertex_offset += mesh.vertices.len() as u32;
    }

    (all_vertices, all_indices)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::io::Write;
    use tempfile::NamedTempFile;

    #[test]
    fn test_simple_obj_loading() {
        let obj_content = r#"
# Simple cube
o cube
v -1.0 -1.0 -1.0
v  1.0 -1.0 -1.0
v  1.0  1.0 -1.0
v -1.0  1.0 -1.0
f 1 2 3 4
"#;

        let mut temp_file = NamedTempFile::new().unwrap();
        temp_file.write_all(obj_content.as_bytes()).unwrap();
        temp_file.flush().unwrap();

        let path = temp_file.path();
        let expected = path
            .file_stem()
            .and_then(|s| s.to_str())
            .unwrap_or("")
            .to_string();
        let model = ObjLoader::load(path).unwrap();

        assert_eq!(model.name, expected);
        assert_eq!(model.meshes.len(), 1);
        assert_eq!(model.meshes[0].vertices.len(), 4);
        assert_eq!(model.meshes[0].indices.len(), 6); // 2 triangles
    }
}
