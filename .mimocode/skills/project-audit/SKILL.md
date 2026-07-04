---
name: project-audit
description: "Thoroughly explore a project codebase and produce a structured analysis with production-readiness recommendations. Use when the user asks to understand, audit, review, or assess a project for quality, readiness, or improvements."
---

# Project Audit Skill

Systematically explore a project codebase and produce a comprehensive analysis covering architecture, tech stack, code quality, build system, dependencies, documentation, and production readiness.

## When to use

- User asks to "explore the project thoroughly" or "understand the codebase"
- User asks for production-readiness assessment
- User asks for code quality review or architecture analysis
- User asks to "suggest improvements" for a project
- Onboarding to an unfamiliar codebase

## Workflow

### Phase 1: Project Identity & Documentation

Read all top-level documentation to understand what the project is:

1. **README.md** — project description, features, build instructions
2. **Architecture docs** (ARCHITECTURE.md, design docs, ADRs)
3. **Project rules** (PROJECT_RULES.md, .editorconfig, contributing guides)
4. **Roadmap/plans** (ROADMAP.md, RELEASE_PLAN.md, TODO files)
5. **Package manifests** (package.json, CMakeLists.txt, vcpkg.json, Cargo.toml, go.mod, etc.)

Output: Project identity summary (what it is, goal, current phase/version).

### Phase 2: Directory Structure & Tech Stack

Map the full project structure:

1. List all top-level directories and key subdirectories
2. Identify tech stack: languages, frameworks, build tools, dependency managers
3. Find entry points (main files, index files, app bootstraps)
4. Identify configuration files (build configs, CI/CD, linting, formatting)

Use `glob` for pattern-based discovery:
- `**/*.cpp`, `**/*.h`, `**/*.py`, `**/*.ts`, etc. for source files
- `**/CMakeLists.txt`, `**/Makefile`, `**/package.json` for build files
- `**/.github/**`, `**/.gitlab-ci.yml` for CI/CD
- `**/Dockerfile*`, `**/docker-compose*` for containerization
- `**/.env*` for environment config

Output: Directory tree overview, tech stack summary, entry points.

### Phase 3: Source Code Analysis

Read key source files to understand code quality:

1. **Entry point** — main file, application bootstrap
2. **Core modules** — key source files in each major directory
3. **Configuration** — build system files, dependency manifests
4. **Tests** — test files, test configuration, coverage setup

Look for:
- Code organization patterns (clean architecture, MVC, etc.)
- Error handling patterns
- Logging and observability
- Security patterns (auth, validation, secrets management)
- TODO/FIXME/HACK comments indicating technical debt
- Test coverage approach

Output: Code quality assessment, patterns observed, technical debt inventory.

### Phase 4: Build System & Dependencies

Analyze build and dependency management:

1. Build system configuration (CMake, Make, Gradle, npm, etc.)
2. Dependency manifest — list all dependencies with versions
3. Build scripts and automation
4. Release/packaging scripts
5. CI/CD pipeline configuration

Check for:
- Outdated or vulnerable dependencies
- Missing build configurations
- No CI/CD pipeline
- No containerization
- Hardcoded paths or configurations

Output: Build system assessment, dependency health, deployment readiness.

### Phase 5: Documentation & Testing

Assess documentation and test coverage:

1. API documentation (generated or hand-written)
2. Inline code documentation
3. Test files and test framework setup
4. Example code or usage guides
5. Video/demo recordings

Output: Documentation completeness, test coverage assessment.

### Phase 6: Production Readiness Gaps

Synthesize findings into production-readiness categories:

| Category | Check |
|----------|-------|
| **CI/CD** | Automated build, test, deploy pipeline |
| **Containerization** | Dockerfile, docker-compose |
| **Environment Config** | .env support, config management |
| **Logging** | Structured logging, log levels |
| **Monitoring** | Health checks, metrics, alerts |
| **Security** | Auth, input validation, secrets mgmt |
| **Testing** | Unit, integration, e2e tests |
| **Documentation** | API docs, README, architecture |
| **Error Handling** | Graceful degradation, error recovery |
| **Performance** | Profiling, optimization, caching |

### Phase 7: Final Report

Produce a structured report with these sections:

```markdown
# Project Analysis: <Project Name>

## 1. Project Overview
<What it is, goal, version, author>

## 2. Directory Structure
<Tree overview>

## 3. Tech Stack
<Languages, frameworks, dependencies>

## 4. Architecture
<Design patterns, module organization>

## 5. Code Quality Assessment
<Patterns, anti-patterns, technical debt>

## 6. Build System & Dependencies
<Build config, dependency health>

## 7. Documentation & Tests
<Coverage, completeness>

## 8. Production Readiness Gaps
<Table of gaps by category>

## 9. Recommendations
<Prioritized list of improvements>

## 10. Current Phase Status
<What's done, what's in progress, what's missing>
```

## Guidelines

- **Be thorough but focused**: Read the most important files fully, scan the rest with glob/grep
- **Use parallel reads**: Read multiple files in parallel when exploring
- **Respect plan mode**: If plan mode is active, only read files and produce analysis — no edits
- **Match user language**: Respond in the same language the user communicates in
- **Cite file paths**: Reference specific files and line numbers in findings
- **Prioritize recommendations**: Rank improvements by impact and effort
- **Acknowledge strengths**: Note what's done well, not just gaps
