import os
import unittest

mount_point = './mount'

def empty_directory(directory):
    for filename in os.listdir(directory):
        file_path = os.path.join(directory, filename)
        if os.path.isfile(file_path):
            os.remove(file_path)
        elif os.path.isdir(file_path):
            empty_directory(file_path)
            os.rmdir(file_path)

class FileSystemTests(unittest.TestCase):
    def setUp(self) -> None:
        empty_directory(mount_point)

    def test_mkdir(self):
        test_dir = os.path.join(mount_point, 'test_directory')
        os.makedirs(test_dir)
        self.assertTrue(os.path.isdir(test_dir))
        os.rmdir(test_dir)

    def test_file_creation(self):
        test_file = os.path.join(mount_point, 'test_file0.txt')
        with open(test_file, 'w') as f:
            pass
        self.assertTrue(os.path.isfile(test_file))
        os.remove(test_file)

    def test_append_to_file(self):
        test_file = os.path.join(mount_point, 'test_file1.txt')
        with open(test_file, 'w') as f:
            f.write('Hello')
        with open(test_file, 'a') as f:
            f.write(' World')
        with open(test_file, 'r') as f:
            content = f.read()
        self.assertEqual(content, 'Hello World')
        os.remove(test_file)

    def test_read_from_file(self):
        test_file = os.path.join(mount_point, 'test_file2.txt')
        with open(test_file, 'w') as f:
            f.write('Testing file read')
        with open(test_file, 'r') as f:
            content = f.read()
        self.assertEqual(content, 'Testing file read')
        os.remove(test_file)

    def test_remove_directory(self):
        test_dir = os.path.join(mount_point, 'test_directory1')
        os.makedirs(test_dir)
        self.assertTrue(os.path.isdir(test_dir))
        os.rmdir(test_dir)
        self.assertFalse(os.path.exists(test_dir))

    def test_hard_links(self):
        test_file = os.path.join(mount_point, 'test_file3.txt')
        with open(test_file, 'w') as f:
            f.write('Testing hard links')

        hard_link_path = os.path.join(mount_point, 'hard_link.txt')
        os.link(test_file, hard_link_path)

        self.assertTrue(os.path.isfile(hard_link_path))
        with open(hard_link_path, 'r') as f:
            hard_link_content = f.read()
        with open(test_file, 'r') as f:
            original_file_content = f.read()
        self.assertEqual(hard_link_content, original_file_content)
        with open(test_file, 'a') as f:
            f.write(' - Modified')

        with open(hard_link_path, 'r') as f:
            modified_content = f.read()
        expected_modified_content = original_file_content + ' - Modified'
        self.assertEqual(modified_content, expected_modified_content)

        os.remove(hard_link_path)
        self.assertFalse(os.path.exists(hard_link_path))
        self.assertTrue(os.path.exists(test_file))
        os.remove(test_file)

    def test_large_file_write(self):
        test_file = os.path.join(mount_point, 'large_file1.txt')
        content = 'A' * 5000

        with open(test_file, 'w') as f:
            f.write(content)

        self.assertTrue(os.path.isfile(test_file))
        with open(test_file, 'r') as f:
            file_content = f.read()

        self.assertEqual(file_content, content)
        os.remove(test_file)

    def test_multiple_directories(self):
        test_dir1 = os.path.join(mount_point, 'test_directory1')
        test_dir2 = os.path.join(test_dir1, 'test_directory2')
        test_dir3 = os.path.join(test_dir2, 'test_directory3')

        os.makedirs(test_dir1)
        self.assertTrue(os.path.isdir(test_dir1))
        os.makedirs(test_dir2)
        self.assertTrue(os.path.isdir(test_dir2))
        os.makedirs(test_dir3)
        self.assertTrue(os.path.isdir(test_dir3))

        test_file = os.path.join(test_dir3, 'test_file5.py')
        with open(test_file, 'w') as f:
            pass

        self.assertTrue(os.path.isfile(test_file))
        os.remove(test_file)
        os.rmdir(test_dir3)
        os.rmdir(test_dir2)
        os.rmdir(test_dir1)

if __name__ == '__main__':
    unittest.main()
