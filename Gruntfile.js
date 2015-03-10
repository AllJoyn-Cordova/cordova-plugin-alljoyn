module.exports = function (grunt) {
    // load all grunt tasks matching the `grunt-*` pattern
    require('load-grunt-tasks')(grunt);

    var files = ['www/*.js', 'src/windows/*.js', 'scripts/*.js', 'tests/*.js'];

    grunt.initConfig({
        jshint: {
            files: files,
            options: {
                jshintrc: '.jshintrc'
            }
        },
        jscs: {
            files: {
                src: files
            },
            options: {
                config: '.jscsrc'
            }
        }
    });

    grunt.registerTask('codestyle', ['jshint', 'jscs']);
    grunt.registerTask('test', ['codestyle']);
    grunt.registerTask('default', ['test']);
};
