/** @type {import('tailwindcss').Config} */
export default {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  theme: {
    extend: {
      colors: {
        kenga: {
          bg: "#0A0E12",   // black-ish
          blue: "#1E88E5", // blue
          green: "#2ECC71",// green
          white: "#FFFFFF" // white
        }
      }
    },
  },
  plugins: [],
}
